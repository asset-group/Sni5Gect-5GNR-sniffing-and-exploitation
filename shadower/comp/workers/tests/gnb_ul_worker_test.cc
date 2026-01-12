#include "shadower/comp/workers/gnb_ul_worker.h"
#include "shadower/comp/workers/ue_dl_worker.h"
#include "shadower/comp/workers/wd_worker.h"
#include "shadower/modules/dummy_exploit.h"
#include "shadower/utils/arg_parser.h"
#include "shadower/utils/phy_cfg_utils.h"
#include "shadower/utils/safe_queue.h"
#include "shadower/utils/utils.h"
#include "srsran/support/srsran_test.h"
std::string    sample_file;
std::string    mib_config_file;
std::string    sib_config_file;
uint32_t       sib_config_size;
std::string    rrc_setup_config_file;
uint32_t       rrc_setup_config_size;
std::string    dci_sample_file;
std::string    last_sample_file;
uint32_t       dci_slot_idx = 0;
uint32_t       ul_slot_idx  = 0;
uint32_t       ncellid      = 1;
uint16_t       rnti         = 0xffff;
ShadowerConfig config       = {};
int32_t        ul_offset    = 0;

SafeQueue<std::vector<uint8_t> > dl_msg_queue;
SafeQueue<std::vector<uint8_t> > ul_msg_queue;

void parse_args(int argc, char* argv[])
{
  int opt;
  while ((opt = getopt(argc, argv, "sfbcRFiIzZyYdDpmoluU")) != -1) {
    switch (opt) {
      case 's': {
        double srateMHz    = atof(argv[optind]);
        config.sample_rate = srateMHz * 1e6;
        printf("Using sample rate: %f MHz\n", srateMHz);
        break;
      }
      case 'f': {
        double freqMHz = atof(argv[optind]);
        config.dl_freq = freqMHz * 1e6;
        config.ul_freq = config.dl_freq;
        printf("Using center frequency: %f MHz\n", config.dl_freq);
        break;
      }
      case 'b': {
        double ssbFreqMHz = atof(argv[optind]);
        config.ssb_freq   = ssbFreqMHz * 1e6;
        printf("Using SSB Frequency: %f MHz\n", config.ssb_freq);
        break;
      }
      case 'c': {
        config.nof_channels = atoi(argv[optind]);
        printf("Using number of channels: %u\n", config.nof_channels);
        break;
      }
      case 'l': {
        last_sample_file = argv[optind];
        printf("Using last sample file: %s\n", last_sample_file.c_str());
        break;
      }
      case 'R': {
        rnti = atoi(argv[optind]);
        printf("Using RNTI: %u\n", rnti);
        break;
      }
      case 'u': {
        config.uplink_cfo = -atof(argv[optind]);
        printf("Using uplink CFO: %f\n", config.uplink_cfo);
        break;
      }
      case 'U': {
        config.uplink_cfo = atof(argv[optind]);
        printf("Using uplink CFO: %f\n", config.uplink_cfo);
        break;
      }
      case 'F': {
        sample_file = argv[optind];
        printf("Using sample file: %s\n", sample_file.c_str());
        break;
      }
      case 'i': {
        ul_slot_idx = atoi(argv[optind]);
        printf("Using slot index: %u\n", ul_slot_idx);
        break;
      }
      case 'o': {
        ul_offset = atoi(argv[optind]);
        printf("Setting uplink offset to: %u\n", ul_offset);
        break;
      }
      case 'I': {
        ncellid = atoi(argv[optind]);
        printf("Using cell id: %u\n", ncellid);
        break;
      }
      case 'm': {
        mib_config_file = argv[optind];
        printf("Using mib config file: %s\n", mib_config_file.c_str());
        break;
      }
      case 'y': {
        rrc_setup_config_file = argv[optind];
        printf("Using RRC setup config file: %s\n", rrc_setup_config_file.c_str());
        break;
      }
      case 'Y': {
        rrc_setup_config_size = atoi(argv[optind]);
        printf("Using RRC setup config size: %u\n", rrc_setup_config_size);
        break;
      }
      case 'z': {
        sib_config_file = argv[optind];
        printf("Using sib config file: %s\n", sib_config_file.c_str());
        break;
      }
      case 'Z': {
        sib_config_size = atoi(argv[optind]);
        printf("Using SIB config size: %u\n", sib_config_size);
        break;
      }
      case 'p': {
        config.nof_prb = atoi(argv[optind]);
        printf("Using number of PRBs: %u\n", config.nof_prb);
        break;
      }
      case 'd': {
        dci_sample_file = argv[optind];
        printf("Using DCI sample file: %s\n", dci_sample_file.c_str());
        break;
      }
      case 'D': {
        dci_slot_idx = atoi(argv[optind]);
        printf("Using DCI slot index: %u\n", dci_slot_idx);
        break;
      }
      default:
        fprintf(stderr, "Unknown option or missing argument.\n");
        exit(EXIT_FAILURE);
    }
  }
  config.ssb_pattern = srsran_ssb_pattern_t::SRSRAN_SSB_PATTERN_C;
  config.duplex_mode = srsran_duplex_mode_t::SRSRAN_DUPLEX_MODE_TDD;
  config.scs_ssb     = srsran_subcarrier_spacing_t::srsran_subcarrier_spacing_30kHz;
  config.scs_common  = srsran_subcarrier_spacing_t::srsran_subcarrier_spacing_30kHz;
  if (sample_file.empty()) {
    fprintf(stderr, "Sample file is required.\n");
    exit(EXIT_FAILURE);
  }
}

int main(int argc, char* argv[])
{
  parse_args(argc, argv);
  /* initialize logger */
  config.log_level             = srslog::basic_levels::debug;
  config.bc_worker_level       = srslog::basic_levels::debug;
  srslog::basic_logger& logger = srslog_init(&config);
  uint32_t              sf_len = config.sample_rate * SF_DURATION;

  /* initialize phy cfg */
  srsran::phy_cfg_nr_t phy_cfg = {};
  init_phy_cfg(phy_cfg, config);

  /* Update phy cfg with mib configurations */

  if (!mib_config_file.empty() && !configure_phy_cfg_from_mib(phy_cfg, mib_config_file, ncellid)) {
    logger.error("Failed to configure phy cfg from mib");
    return -1;
  }
  /* Update phy cfg with sib configurations */
  if (!sib_config_file.empty() && !configure_phy_cfg_from_sib1(phy_cfg, sib_config_file, sib_config_size)) {
    logger.error("Failed to configure phy cfg from SIB1");
    return -1;
  }

  /* Update phy cfg with rrc setup configurations */
  if (!rrc_setup_config_file.empty() &&
      !configure_phy_cfg_from_rrc_setup(phy_cfg, rrc_setup_config_file, rrc_setup_config_size, logger)) {
    logger.error("Failed to configure phy cfg from RRC setup");
    return -1;
  }

  /* init phy state */
  srsue::nr::state phy_state = {};
  init_phy_state(phy_state, config.nof_prb);

  /* Initialize the wd worker */
  srsran_duplex_mode_t duplex_mode = srsran_duplex_mode_t::SRSRAN_DUPLEX_MODE_TDD;
  WDWorker             wd_worker(duplex_mode, config.log_level);

  /* initialize exploit */
  DummyExploit exploit(dl_msg_queue, ul_msg_queue);

  /* Initialize the pcap writer */
  std::shared_ptr<srsran::mac_pcap> pcap_writer = std::make_unique<srsran::mac_pcap>();
  std::string                       pcap_file   = "/tmp/test.pcap";
  if (pcap_writer->open(pcap_file)) {
    logger.error("Failed to open pcap file");
    return -1;
  }

  /* Initialize the ue_dl worker */
  UEDLWorker* ue_dl_worker = new UEDLWorker(logger, config, phy_state, &wd_worker, &exploit, pcap_writer);
  ue_dl_worker->init(phy_cfg);
  ue_dl_worker->update_cfg(phy_cfg);
  ue_dl_worker->set_rnti(rnti, srsran_rnti_type_t::srsran_rnti_type_c);
  ue_dl_worker->update_pending_rrc_setup(false);

  /* Look for DCIs */
  std::shared_ptr<Task>               dci_task    = std::make_shared<Task>();
  std::shared_ptr<std::vector<cf_t> > dci_samples = std::make_shared<std::vector<cf_t> >(sf_len);
  if (!load_samples(dci_sample_file, dci_samples->data(), sf_len)) {
    logger.error("Failed to load samples\n");
    return -1;
  }
  dci_task->dl_buffer[0] = dci_samples;
  dci_task->slot_idx     = dci_slot_idx;
  dci_task->ts           = {};
  ue_dl_worker->process_task(dci_task);

  /* Initialize the gnb_ul worker */
  GNBULWorker* gnb_ul_worker = new GNBULWorker(logger, config, phy_state, &wd_worker, &exploit, pcap_writer);
  gnb_ul_worker->init(phy_cfg);
  gnb_ul_worker->update_cfg(phy_cfg);
  gnb_ul_worker->set_rnti(rnti, srsran_rnti_type_t::srsran_rnti_type_c);
  gnb_ul_worker->set_ta_samples(ul_offset);

  /* Load the IQ samples from file */
  std::shared_ptr<Task>               ul_task = std::make_shared<Task>();
  std::shared_ptr<std::vector<cf_t> > samples = std::make_shared<std::vector<cf_t> >(sf_len);
  if (!load_samples(sample_file, samples->data(), sf_len)) {
    logger.error("Failed to load samples\n");
    return -1;
  }
  std::shared_ptr<std::vector<cf_t> > last_samples = std::make_shared<std::vector<cf_t> >(sf_len);
  if (!last_sample_file.empty()) {
    if (!load_samples(last_sample_file, last_samples->data(), sf_len)) {
      logger.error("Failed to load last samples\n");
      return -1;
    }
    ul_task->last_ul_buffer[0] = last_samples;
  } else {
    ul_task->last_ul_buffer[0] = samples;
  }
  ul_task->ul_buffer[0] = samples;
  ul_task->slot_idx     = ul_slot_idx;
  ul_task->ts           = {};
  gnb_ul_worker->process_task(ul_task);
  pcap_writer->close();
  return 0;
}