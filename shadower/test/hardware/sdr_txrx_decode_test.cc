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

SafeQueue<Task>    task_queue = {};
std::atomic<bool>  running{true};
std::atomic<bool>  cell_found{false};
uint32_t           test_round      = 100;
uint32_t           target_slot_idx = 7;
uint16_t           rnti            = 0x4601;
uint32_t           cell_id         = 1;
srsran_rnti_type_t rnti_type       = srsran_rnti_type_c;
uint32_t           total_send      = 0;
uint32_t           total_decoded   = 0;
uint32_t           dci_decoded     = 0;

const uint8_t message_to_send[] = {0x01, 0x03, 0x00, 0x01, 0x00, 0x01, 0x0f, 0xc0, 0x00, 0x00, 0x00, 0x28, 0x80,
                                   0x8f, 0xc0, 0x0b, 0x60, 0x20, 0x00, 0x00, 0x00, 0x00, 0x3f, 0x00, 0x00};

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

int generate_test_waveform(srslog::basic_logger& logger,
                           ShadowerConfig&       config,
                           srsran::phy_cfg_nr_t& phy_cfg,
                           uint32_t              slot_len,
                           cf_t*                 buffer)
{
  /* GNB DL init with configuration from phy_cfg */
  srsran_gnb_dl_t gnb_dl        = {};
  cf_t*           gnb_dl_buffer = srsran_vec_cf_malloc(slot_len);
  if (!init_gnb_dl(gnb_dl, gnb_dl_buffer, phy_cfg, config.sample_rate)) {
    logger.error("Failed to init GNB DL");
    return -1;
  }

  /* Initialize softbuffer tx */
  srsran_softbuffer_tx_t softbuffer_tx = {};
  if (srsran_softbuffer_tx_init_guru(&softbuffer_tx, SRSRAN_SCH_NR_MAX_NOF_CB_LDPC, SRSRAN_LDPC_MAX_LEN_ENCODED_CB) <
      SRSRAN_SUCCESS) {
    logger.error("Error initializing softbuffer_tx");
    return -1;
  }
  /* Zero out the gnb_dl resource grid */
  if (srsran_gnb_dl_base_zero(&gnb_dl) < SRSRAN_SUCCESS) {
    logger.error("Error zero RE grid of gNB DL");
    return -1;
  }

  /* Update pdcch with dci_cfg */
  srsran_dci_cfg_nr_t dci_cfg = phy_cfg.get_dci_cfg();
  if (srsran_gnb_dl_set_pdcch_config(&gnb_dl, &phy_cfg.pdcch, &dci_cfg) < SRSRAN_SUCCESS) {
    logger.error("Error setting PDCCH config for gnb dl");
    return -1;
  }
  srsran_slot_cfg_t slot_cfg = {.idx = target_slot_idx};

  /* Build the DCI message */
  srsran_dci_dl_nr_t dci_to_send = {};
  if (!construct_dci_dl_to_send(
          dci_to_send, phy_cfg, slot_cfg.idx, rnti, rnti_type, config.pdsch_mcs, config.pdsch_prbs)) {
    logger.error("Error constructing DCI to send");
    return -1;
  }

  /* Pack dci into pdcch */
  if (srsran_gnb_dl_pdcch_put_dl(&gnb_dl, &slot_cfg, &dci_to_send) < SRSRAN_SUCCESS) {
    logger.error("Error putting DCI into PDCCH");
    return -1;
  }
  /* get pdsch_cfg from phy_cfg */
  srsran_sch_cfg_nr_t pdsch_cfg_gnb_dl = {};
  if (!phy_cfg.get_pdsch_cfg(slot_cfg, dci_to_send, pdsch_cfg_gnb_dl)) {
    logger.error("Error getting PDSCH config from phy_cfg");
    return -1;
  }

  /* Initialize the data buffer */
  uint8_t* data_tx[SRSRAN_MAX_TB] = {};
  data_tx[0]                      = srsran_vec_u8_malloc(SRSRAN_SLOT_MAX_NOF_BITS_NR);
  if (data_tx[0] == nullptr) {
    logger.error("Error allocating data buffer");
    return -1;
  }

  /* Copy the message bytes into the data buffer */
  memset(data_tx[0], 0, SRSRAN_SLOT_MAX_NOF_BITS_NR);
  memcpy(data_tx[0], message_to_send, sizeof(message_to_send));

  /* put the message into pdsch */
  pdsch_cfg_gnb_dl.grant.tb[0].softbuffer.tx = &softbuffer_tx;
  if (srsran_gnb_dl_pdsch_put(&gnb_dl, &slot_cfg, &pdsch_cfg_gnb_dl, data_tx) < SRSRAN_SUCCESS) {
    logger.error("Error putting PDSCH message");
    return -1;
  }

  /* Encode the message into IQ samples */
  srsran_softbuffer_tx_reset(&softbuffer_tx);
  srsran_gnb_dl_gen_signal(&gnb_dl);

  /* Write the samples to file */
  char filename[64];
  sprintf(filename, "gnb_dl_buffer");
  write_record_to_file(gnb_dl_buffer, slot_len, filename);

  srsran_vec_cf_copy(buffer, gnb_dl_buffer, slot_len);
  srsran_gnb_dl_free(&gnb_dl);
  free(gnb_dl_buffer);
  return 0;
}

/* Exit on syncer error, and also stop the sender thread */
void handle_syncer_exit(Syncer* syncer, std::thread& sender, std::vector<std::thread>& receivers)
{
  running = false;
  syncer->thread_cancel();
  pthread_cancel(sender.native_handle());
  for (auto& receiver : receivers) {
    pthread_cancel(receiver.native_handle());
  }
}

void sender_thread(srslog::basic_logger& logger,
                   std::vector<cf_t>     samples,
                   uint32_t              num_samples,
                   Source*               source,
                   Syncer*               syncer,
                   ShadowerConfig&       config)
{
  cf_t* channels_ptr[SRSRAN_MAX_CHANNELS];
  for (int i = 0; i < SRSRAN_MAX_CHANNELS; i++) {
    if (i < source->nof_channels) {
      channels_ptr[i] = samples.data();
    } else {
      channels_ptr[i] = nullptr;
    }
  }

  uint32_t slot_per_frame = 10 * (1 << config.scs_ssb);
  uint32_t last_sent_slot = 0;
  auto     last_send_time = std::chrono::steady_clock::now();
  while (running) {
    /* Wait the cell be found first, we are aiming to matching the time of a base station */
    if (!cell_found) {
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
      continue;
    }
    /* Get the timestamp from syncer */
    uint32_t           slot_idx = 0;
    srsran_timestamp_t ts       = {};
    syncer->get_tti(&slot_idx, &ts);
    if (slot_idx % slot_per_frame != 4) {
      continue;
    }
    if (slot_idx == last_sent_slot) {
      std::this_thread::sleep_for(std::chrono::microseconds(100));
      continue;
    }

    uint32_t target_slot_idx = slot_idx + 4;
    srsran_timestamp_add(&ts, 0, 2e-3 - (config.tx_advancement + config.front_padding) / config.sample_rate);
    source->send(channels_ptr, num_samples, ts, target_slot_idx);
    last_sent_slot = slot_idx;

    auto now          = std::chrono::steady_clock::now();
    auto schedule_gap = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_send_time);
    last_send_time    = now;
    if (schedule_gap.count() > 10) {
      logger.warning("Sender thread is running behind, schedule gap: %ld ms", schedule_gap.count());
    }
    total_send++;
    if (total_send % test_round == 0) {
      double success_rate = total_decoded / (double)total_send * 100;
      double dci_rate     = dci_decoded / (double)total_send * 100;
      double depend_rate  = total_decoded / (double)dci_decoded * 100;
      logger.info(
          "TX advancement: %u Total sent: %u, DCI decoded: %u (%.2f%%) PDSCH decoded: %u (%.2f%%) PDSCH/DCI %.2f%%",
          config.tx_advancement,
          total_send,
          dci_decoded,
          dci_rate,
          total_decoded,
          success_rate,
          depend_rate);
    }
  }
}

void receiver_thread(srslog::basic_logger& logger,
                     ShadowerConfig&       config,
                     srsran::phy_cfg_nr_t& phy_cfg,
                     uint32_t              slot_len)
{
  /* init phy state */
  srsue::nr::state phy_state = {};
  init_phy_state(phy_state, config.nof_prb);
  uint32_t sf_len = config.sample_rate * SF_DURATION;

  /* UE DL init with configuration from phy_cfg */
  srsran_ue_dl_nr_t ue_dl        = {};
  cf_t*             ue_dl_buffer = srsran_vec_cf_malloc(sf_len);
  if (!init_ue_dl(ue_dl, ue_dl_buffer, phy_cfg)) {
    logger.error("Failed to init UE DL");
    return;
  }

  /* Initialize softbuffer rx */
  srsran_softbuffer_rx_t softbuffer_rx = {};
  if (srsran_softbuffer_rx_init_guru(&softbuffer_rx, SRSRAN_SCH_NR_MAX_NOF_CB_LDPC, SRSRAN_LDPC_MAX_LEN_ENCODED_CB) <
      SRSRAN_SUCCESS) {
    logger.error("Error initializing softbuffer_rx");
    return;
  }

  /* Initialize the buffer for output*/
  srsran::unique_byte_buffer_t data = srsran::make_byte_buffer();
  if (data == nullptr) {
    logger.error("Error creating byte buffer");
    return;
  }

  srsran_slot_cfg_t                   slot_cfg = {.idx = target_slot_idx};
  std::shared_ptr<std::vector<cf_t> > buffer   = std::make_shared<std::vector<cf_t> >(sf_len);
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

    /* Copy the data to the processing buffer */
    srsran_vec_cf_copy(ue_dl_buffer, task->dl_buffer[0]->data(), sf_len);

    /* run ue_dl estimate fft */
    srsran_ue_dl_nr_estimate_fft(&ue_dl, &slot_cfg);

    std::array<srsran_dci_dl_nr_t, SRSRAN_SEARCH_SPACE_MAX_NOF_CANDIDATES_NR> dci_dl = {};
    std::array<srsran_dci_ul_nr_t, SRSRAN_SEARCH_SPACE_MAX_NOF_CANDIDATES_NR> dci_ul = {};
    /* search for dci */
    ue_dl_dci_search(ue_dl, phy_cfg, slot_cfg, rnti, rnti_type, phy_state, logger, 0, dci_dl, dci_ul);

    /* get grant from dci search */
    uint32_t                   pid             = 0;
    srsran_sch_cfg_nr_t        pdsch_cfg_ue_dl = {};
    srsran_harq_ack_resource_t ack_resource    = {};
    if (!phy_state.get_dl_pending_grant(slot_cfg.idx, pdsch_cfg_ue_dl, ack_resource, pid)) {
      // logger.debug("Failed to get grant from dci search");
      continue;
    }
    dci_decoded++;

    // char filename[64];
    // sprintf(filename, "decoded_msg_%u", dci_decoded);
    // write_record_to_file(ue_dl_buffer, slot_len, filename);

    // sprintf(filename, "ofdm_decoded_fft1272_%u", dci_decoded);
    // write_record_to_file(ue_dl.sf_symbols[0], 1272 * 14, filename);

    /* Initialize pdsch result*/
    srsran_pdsch_res_nr_t pdsch_res = {};
    pdsch_res.tb[0].payload         = data->msg;
    srsran_softbuffer_rx_reset(&softbuffer_rx);
    data->N_bytes = pdsch_cfg_ue_dl.grant.tb[0].tbs / 8U;

    /* Decode PDSCH */
    if (!ue_dl_pdsch_decode(ue_dl, pdsch_cfg_ue_dl, slot_cfg, pdsch_res, softbuffer_rx, logger)) {
      continue;
    }

    /* if the message is not decoded correctly, then return */
    if (!pdsch_res.tb[0].crc) {
      logger.debug("Error PDSCH got wrong CRC");
      continue;
    }
    logger.debug("PDSCH decoded successfully");
    total_decoded++;
  }
  srsran_ue_dl_nr_free(&ue_dl);
  free(ue_dl_buffer);
  return;
}

void parse_args(int argc, char* argv[], ShadowerConfig& config)
{
  int opt;
  while ((opt = getopt(argc, argv, "sdPSrtcFb")) != -1) {
    switch (opt) {
      case 's':
        config.sample_rate = atof(argv[optind]) * 1e6;
        break;
      case 'd':
        config.source_params = argv[optind];
        break;
      case 'P':
        config.ssb_period = atoi(argv[optind]);
        break;
      case 'S':
        config.scs_ssb = srsran_subcarrier_spacing_from_str(argv[optind]);
        break;
      case 'r':
        test_round = atoi(argv[optind]);
        break;
      case 't':
        config.tx_advancement = atoi(argv[optind]);
        break;
      case 'c':
        config.nof_channels = atoi(argv[optind]);
        break;
      case 'F':
        config.front_padding = atoi(argv[optind]);
        break;
      case 'b':
        config.back_padding = atoi(argv[optind]);
        break;
      default:
        fprintf(stderr, "Unknown option: %c\n", opt);
        exit(EXIT_FAILURE);
    }
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
  logger.set_level(srslog::basic_levels::info);
  config.enable_recorder = true;

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

  /* Initialize source */
  create_source_t uhd_source = load_source(uhd_source_module_path);
  config.channels.resize(config.nof_channels);
  for (uint32_t ch = 0; ch < config.nof_channels; ch++) {
    config.channels[ch].rx_frequency = config.dl_freq;
    config.channels[ch].tx_frequency = config.ul_freq;
    config.channels[ch].rx_gain      = 40;
    config.channels[ch].tx_gain      = 80;
    config.channels[ch].enabled      = true;
  }
  Source* source = uhd_source(config);

  std::vector<cf_t> samples_to_inject(args.sf_len * 2);
  if (generate_test_waveform(logger, config, phy_cfg, args.slot_len, samples_to_inject.data()) != 0) {
    logger.error("Failed to generate test waveform");
    return -1;
  }
  // Shift the samples with a front-padding
  uint32_t num_samples = args.slot_len + config.front_padding;
  srsran_vec_cf_copy(samples_to_inject.data() + config.front_padding, samples_to_inject.data(), args.slot_len);
  srsran_vec_cf_zero(samples_to_inject.data(), config.front_padding);
  srsran_vec_cf_zero(samples_to_inject.data() + num_samples, config.back_padding);
  num_samples += config.back_padding;

  // Scale the IQ samples
  float scale = 1.0f;
  srsran_vec_sc_prod_cfc(samples_to_inject.data() + config.front_padding,
                         scale,
                         samples_to_inject.data() + config.front_padding,
                         args.slot_len);

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
  /* Sender thread keep sending waveform */
  std::thread sender(
      sender_thread, std::ref(logger), std::ref(samples_to_inject), num_samples, source, syncer, std::ref(config));
  pthread_setschedparam(sender.native_handle(), SCHED_OTHER, &param);

  /* Receiver thread keep processing sent SSB blocks */
  int                      num_receiver_threads = 4;
  std::vector<std::thread> receiver_threads;
  for (int i = 0; i < num_receiver_threads; i++) {
    std::thread receiver(receiver_thread, std::ref(logger), std::ref(config), std::ref(phy_cfg), args.slot_len);
    receiver_threads.push_back(std::move(receiver));
  }
  syncer->error_handler = std::bind([&]() { handle_syncer_exit(syncer, sender, receiver_threads); });
  syncer->start(0);
  syncer->wait_thread_finish();
  sender.join();
  for (auto& receiver : receiver_threads) {
    receiver.join();
  }
  source->close();
}