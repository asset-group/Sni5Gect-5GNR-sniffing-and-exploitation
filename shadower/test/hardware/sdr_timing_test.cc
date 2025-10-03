extern "C" {
#include "srsran/phy/sync/sss_nr.h"
}
#include "shadower/comp/sync/syncer.h"
#include "shadower/test/test_variables.h"
#include "shadower/utils/safe_queue.h"
#include "shadower/utils/utils.h"
#include "srsran/mac/mac_rar_pdu_nr.h"
#include "srsran/phy/phch/pbch_msg_nr.h"
#include "srsran/phy/sync/ssb.h"
#include "srsran/phy/ue/ue_dl_nr.h"
#include <condition_variable>
#include <dirent.h>
#include <fstream>
#include <mutex>
#include <sched.h>
#include <sys/types.h>

SafeQueue<Task>   task_queue = {};
std::atomic<bool> running{true};
std::atomic<bool> cell_found{false};
double            test_ssb_freq   = 3424.8e6;
std::string       source_param    = "type=b200";
uint32_t          advancement     = 9;
uint32_t          ssb_offset      = 1650;
uint32_t          test_round      = 100;
uint32_t          cell_id         = 1;
uint32_t          ssb_period      = 10;
uint32_t          count           = 0;
double            total_cfo       = 0;
bool              enable_receiver = false;
bool              enable_sender   = false;
std::mutex        mtx;

std::map<int32_t, uint32_t> delay_map;

/* When a cell is found log the cell information */
bool on_cell_found(srsran_mib_nr_t& mib, uint32_t ncellid)
{
  std::array<char, 512> mib_info_str = {};
  srsran_pbch_msg_nr_mib_info(&mib, mib_info_str.data(), (uint32_t)mib_info_str.size());
  printf("Found cell: %s\n", mib_info_str.data());
  cell_found = true;
  cell_id    = ncellid;
  return true;
}

/* Syncer function push new task to the queue */
void push_new_task(std::shared_ptr<Task>& task)
{
  task_queue.push(task);
}

int init_ssb(ShadowerConfig& config, srsran_ssb_t& ssb, srsran_ssb_cfg_t& ssb_cfg)
{
  /* initialize SSB */
  srsran_ssb_args_t ssb_args = {};
  ssb_args.max_srate_hz      = config.sample_rate;
  ssb_args.min_scs           = config.scs_ssb;
  ssb_args.enable_search     = true;
  ssb_args.enable_measure    = true;
  ssb_args.enable_decode     = true;
  ssb_args.enable_encode     = true;
  if (srsran_ssb_init(&ssb, &ssb_args) != 0) {
    printf("Failed to initialize ssb\n");
    return -1;
  }

  /* Set SSB config */
  ssb_cfg.srate_hz       = config.sample_rate;
  ssb_cfg.center_freq_hz = config.dl_freq;
  ssb_cfg.ssb_freq_hz    = test_ssb_freq;
  ssb_cfg.scs            = config.scs_ssb;
  ssb_cfg.pattern        = config.ssb_pattern;
  ssb_cfg.duplex_mode    = config.duplex_mode;
  ssb_cfg.periodicity_ms = 10;
  if (srsran_ssb_set_cfg(&ssb, &ssb_cfg) < SRSRAN_SUCCESS) {
    printf("Failed to set ssb config\n");
    return -1;
  }
  return 0;
}

int generate_ssb_block(ShadowerConfig& config, srsran::phy_cfg_nr_t& phy_cfg, uint32_t slot_len, cf_t* buffer)
{
  srsran_ssb_t     ssb     = {};
  srsran_ssb_cfg_t ssb_cfg = {};
  if (init_ssb(config, ssb, ssb_cfg) != 0) {
    printf("Failed to initialize SSB\n");
    return -1;
  }
  /* GNB DL init with configuration from phy_cfg */
  srsran_gnb_dl_t gnb_dl        = {};
  cf_t*           gnb_dl_buffer = srsran_vec_cf_malloc(slot_len);
  if (!init_gnb_dl(gnb_dl, gnb_dl_buffer, phy_cfg, config.sample_rate)) {
    printf("Failed to init GNB DL\n");
    return -1;
  }

  /* Add ssb config to gnb_dl */
  srsran_gnb_dl_set_ssb_config(&gnb_dl, &ssb_cfg);
  srsran_mib_nr_t mib        = {};
  mib.sfn                    = 100;
  mib.ssb_idx                = 0;
  mib.hrf                    = false;
  mib.scs_common             = config.scs_ssb;
  mib.ssb_offset             = 14;
  mib.dmrs_typeA_pos         = srsran_dmrs_sch_typeA_pos_2;
  mib.coreset0_idx           = 2;
  mib.ss0_idx                = 2;
  mib.cell_barred            = false;
  mib.intra_freq_reselection = false;
  mib.spare                  = 0;

  srsran_pbch_msg_nr_t pbch_msg = {};
  if (srsran_pbch_msg_nr_mib_pack(&mib, &pbch_msg) < SRSRAN_SUCCESS) {
    printf("Failed to pack MIB into PBCH message\n");
    return -1;
  }

  if (srsran_gnb_dl_add_ssb(&gnb_dl, &pbch_msg, mib.sfn) < SRSRAN_SUCCESS) {
    printf("Failed to add SSB\n");
    return -1;
  }

  uint32_t ssb_offset_calc = srsran_ssb_candidate_sf_offset(&ssb, 0);
  printf("SSB offset: %u\n", ssb_offset_calc);
  ssb_offset = ssb_offset_calc;

  srsran_vec_cf_copy(buffer, gnb_dl_buffer, slot_len);
  srsran_ssb_free(&ssb);
  srsran_gnb_dl_free(&gnb_dl);
  free(gnb_dl_buffer);
  return 0;
}

/* Exit on syncer error, and also stop the sender thread */
void handle_syncer_exit(Syncer* syncer, std::thread* sender, std::vector<std::thread>& receivers)
{
  running = false;
  syncer->thread_cancel();
  if (sender) {
    pthread_cancel(sender->native_handle());
  }
  for (auto& receiver : receivers) {
    pthread_cancel(receiver.native_handle());
  }
}

void sender_thread(srslog::basic_logger& logger,
                   std::vector<cf_t>&    samples,
                   uint32_t              num_samples,
                   Source*               source,
                   Syncer*               syncer,
                   ShadowerConfig&       config)
{
  cf_t* channels_ptr[SRSRAN_MAX_CHANNELS] = {};
  for (int i = 0; i < SRSRAN_MAX_CHANNELS; i++) {
    channels_ptr[i] = nullptr;
  }
  channels_ptr[0] = samples.data();

  uint32_t slot_per_sf      = 1 << config.scs_ssb;
  uint32_t slot_per_frame   = 10 * slot_per_sf;
  double   slot_duration    = SF_DURATION / slot_per_sf;
  uint32_t slot_advancement = 6;
  uint32_t last_slot        = 0;
  auto     last_send_time   = std::chrono::steady_clock::now();
  uint32_t send_idx         = 0;
  while (running) {
    /* Wait the cell be found first, we are aiming to matching the time of a base station */
    if (!cell_found) {
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
      continue;
    }
    /* Get the timestamp from syncer */
    uint32_t           slot_idx;
    srsran_timestamp_t ts;
    syncer->get_tti(&slot_idx, &ts);
    if (slot_idx % slot_per_frame != 2) {
      continue;
    }
    if (slot_idx == last_slot) {
      std::this_thread::sleep_for(std::chrono::microseconds(100));
      continue;
    }

    /* Send the samples */
    uint32_t target_slot_idx = slot_idx + slot_advancement;
    srsran_timestamp_add(&ts, 0, slot_duration * slot_advancement);
    source->send(channels_ptr, num_samples, ts, target_slot_idx);
    last_slot = slot_idx;

    auto now          = std::chrono::steady_clock::now();
    auto schedule_gap = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_send_time);
    last_send_time    = now;
    if (schedule_gap.count() > 10) {
      logger.warning("Sender thread is running behind, schedule gap: %ld ms", schedule_gap.count());
    }
    send_idx++;
  }
}

void receiver_thread(srslog::basic_logger& logger, ShadowerConfig& config, uint32_t sf_len, double test_freq)
{
  srsran_ssb_t     ssb     = {};
  srsran_ssb_cfg_t ssb_cfg = {};
  if (init_ssb(config, ssb, ssb_cfg) != 0) {
    logger.error("Failed to initialize SSB");
    return;
  }

  std::shared_ptr<std::vector<cf_t> > buffer = std::make_shared<std::vector<cf_t> >(config.sample_rate * SF_DURATION);
  while (running) {
    /* Retrieve the well aligned slots */
    if (!cell_found) {
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
      continue;
    }
    std::shared_ptr<Task> task = task_queue.retrieve();
    if (task == nullptr) {
      continue;
    }
    if (!task->dl_buffer[0]) {
      continue;
    }

    /* Run SSB search on the received slots */
    srsran_ssb_search_res_t res = {};
    if (srsran_ssb_search(&ssb, task->dl_buffer[0]->data(), sf_len, &res) != 0) {
      logger.error("Failed to search ssb");
      continue;
    }
    if (res.measurements.snr_dB < -10.0f || res.measurements.cfo_hz == 0) {
      continue;
    }
    if (res.N_id != cell_id) {
      continue;
    }

    /* decode MIB */
    srsran_mib_nr_t mib = {};
    if (srsran_pbch_msg_nr_mib_unpack(&res.pbch_msg, &mib) < SRSRAN_SUCCESS) {
      logger.error("Error running srsran_pbch_msg_nr_mib_unpack");
      continue;
    }

    /* write MIB to file */
    std::array<char, 512> mib_info_str = {};
    srsran_pbch_msg_nr_mib_info(&mib, mib_info_str.data(), mib_info_str.size());
    logger.debug("SSB Delay: %u %s ", res.t_offset, mib_info_str.data());
    {
      std::lock_guard<std::mutex> lock(mtx);
      count++;
      int32_t delay_samples = res.t_offset - ssb_offset;
      total_cfo += res.measurements.cfo_hz;
      /* Count the delays */
      if (delay_map.find(delay_samples) == delay_map.end()) {
        delay_map[delay_samples] = 1;
      } else {
        delay_map[delay_samples]++;
      }
      if (count % test_round == 0 && count > 1) {
        /* Log the static information */
        int32_t total = 0;
        int32_t min   = 10000;
        int32_t max   = 0;
        for (const auto& pair : delay_map) {
          logger.info("Delay: %d Count: %u", pair.first, pair.second);
          if (pair.first < min) {
            min = pair.first;
          }
          if (pair.first > max) {
            max = pair.first;
          }
          total += pair.first * pair.second;
        }
        logger.info("Min: %d Max: %d Avg: %d", min, max, total / count);
        logger.info("Avg CFO: %f", total_cfo / count);
      }
    }
  }
  srsran_ssb_free(&ssb);
}

void parse_args(int argc, char* argv[], ShadowerConfig& config)
{
  int opt;
  config.channels.resize(1);
  while ((opt = getopt(argc, argv, "dPtTsSfFHgGcrBRZ")) != -1) {
    switch (opt) {
      case 'R':
        enable_receiver = true;
        break;
      case 'Z':
        enable_sender = true;
        break;
      case 'd':
        config.source_params = argv[optind];
        break;
      case 'P':
        config.ssb_period = atoi(argv[optind]);
        break;
      case 't': {
        config.ssb_freq = atof(argv[optind]) * 1e6;
        break;
      }
      case 'T': {
        double test_ssb_freq_MHz = atof(argv[optind]);
        test_ssb_freq            = test_ssb_freq_MHz * 1e6;
        break;
      }
      case 's':
        config.sample_rate = atof(argv[optind]) * 1e6;
        break;
      case 'S': {
        config.scs_ssb = srsran_subcarrier_spacing_from_str(argv[optind]);
        break;
      }
      case 'f': {
        double center_freq_MHz = atof(argv[optind]);
        config.dl_freq         = center_freq_MHz * 1e6;
        config.ul_freq         = center_freq_MHz * 1e6;
        break;
      }
      case 'F': {
        config.channels[0].rx_frequency = atof(argv[optind]) * 1e6;
        break;
      }
      case 'H': {
        config.channels[0].tx_frequency = atof(argv[optind]) * 1e6;
        break;
      }
      case 'g': {
        config.channels[0].rx_gain = atof(argv[optind]);
        break;
      }
      case 'G':
        config.channels[0].tx_gain = atof(argv[optind]);
        break;
      case 'B':
        config.band = atoi(argv[optind]);
        break;
      case 'c': {
        config.nof_channels = atoi(argv[optind]);
        break;
      }
      case 'r':
        test_round = atoi(argv[optind]);
        break;
      default:
        fprintf(stderr, "Unknown option: %c\n", opt);
        exit(EXIT_FAILURE);
    }
  }
  config.channels[0].enabled = true;
  if (config.channels[0].rx_frequency == 0 || config.channels[0].tx_frequency == 0) {
    config.channels[0].rx_frequency = config.dl_freq;
    config.channels[0].tx_frequency = config.ul_freq;
    config.channels[0].rx_gain      = 40;
    config.channels[0].tx_gain      = 80;
  }
}

int main(int argc, char* argv[])
{
  int test_number = 0;
  if (argc > 1) {
    test_number = atoi(argv[1]);
  }
  test_args_t     args   = init_test_args(test_number);
  ShadowerConfig& config = args.config;
  parse_args(argc, argv, config);
  config.syncer_log_level = srslog::basic_levels::info;
  /* initialize logger */
  srslog::basic_logger& logger = srslog_init(&config);
  logger.set_level(srslog::basic_levels::debug);

  /* initialize phy_cfg */
  srsran::phy_cfg_nr_t phy_cfg = {};
  init_phy_cfg(phy_cfg, config);

  /* load mib configuration and update phy_cfg */
  if (!configure_phy_cfg_from_mib(phy_cfg, args.mib_config_raw, args.ncellid)) {
    logger.error("Failed to configure phy cfg from mib");
    return -1;
  }

  /* load sib1 configuration and apply to phy_cfg */
  if (!configure_phy_cfg_from_sib1(phy_cfg, args.sib_config_raw, args.sib_size)) {
    logger.error("Failed to configure phy cfg from sib1");
    return -1;
  }

  /* load rrc setup configuration and apply to phy_cfg */
  if (!configure_phy_cfg_from_rrc_setup(phy_cfg, args.rrc_setup_raw, args.rrc_setup_size, logger)) {
    logger.error("Failed to configure phy cfg from rrc setup");
    return -1;
  }
  config.enable_recorder = false;
  config.channels.resize(config.nof_channels);
  if (config.nof_channels > 1) {
    for (uint32_t i = 1; i < config.nof_channels; i++) {
      config.channels[i] = config.channels[0];
    }
  }

  uint32_t sf_len          = config.sample_rate * SF_DURATION;
  config.slot_per_subframe = 1 << config.scs_ssb;
  args.slot_len            = sf_len / config.slot_per_subframe;

  /* Initialize source */
  create_source_t creator = load_source(uhd_source_module_path);
  Source*         source  = creator(config);
  logger.info("Selected target test SSB frequency %.3f MHz", test_ssb_freq / 1e6);

  std::vector<cf_t> test_ssb_samples(args.sf_len * 2);
  if (generate_ssb_block(config, phy_cfg, args.sf_len, test_ssb_samples.data()) != 0) {
    logger.error("Failed to generate SSB block");
    return -1;
  }

  char filename[64];
  sprintf(filename, "generated_ssb");
  write_record_to_file(test_ssb_samples.data(), args.slot_len, filename);

  int                max_priority = sched_get_priority_max(SCHED_FIFO) - 1;
  struct sched_param param;
  param.sched_priority = max_priority;
  pthread_attr_t attr;

  /* Initialize syncer */
  syncer_args_t syncer_args = {
      .srate       = config.sample_rate,
      .scs         = config.scs_ssb,
      .dl_freq     = config.dl_freq,
      .ssb_freq    = config.ssb_freq,
      .pattern     = config.ssb_pattern,
      .duplex_mode = config.duplex_mode,
  };
  Syncer* syncer = new Syncer(syncer_args, source, config);
  syncer->init();
  syncer->on_cell_found    = std::bind(on_cell_found, std::placeholders::_1, std::placeholders::_2);
  syncer->publish_subframe = std::bind(push_new_task, std::placeholders::_1);

  std::thread* sender_ptr = nullptr;
  if (enable_sender) {
    /* Sender thread keep sending SSB blocks */
    logger.info("Starting sender thread");
    sender_ptr = new std::thread(
        sender_thread, std::ref(logger), std::ref(test_ssb_samples), args.slot_len, source, syncer, std::ref(config));
    pthread_setschedparam(sender_ptr->native_handle(), SCHED_OTHER, &param);
  }

  /* Receiver thread keep processing sent SSB blocks */
  std::vector<std::thread> receiver_threads;
  if (enable_receiver) {
    int num_receiver_threads = 4;
    for (int i = 0; i < num_receiver_threads; i++) {
      logger.info("Starting receiver thread %d", i);
      std::thread receiver(receiver_thread, std::ref(logger), std::ref(config), args.sf_len, test_ssb_freq);
      receiver_threads.push_back(std::move(receiver));
    }
  }

  syncer->error_handler = std::bind([&]() { handle_syncer_exit(syncer, sender_ptr, receiver_threads); });
  syncer->start(0);
  syncer->wait_thread_finish();
  if (sender_ptr) {
    sender_ptr->join();
  }
  for (auto& receiver : receiver_threads) {
    receiver.join();
  }
  source->close();
}