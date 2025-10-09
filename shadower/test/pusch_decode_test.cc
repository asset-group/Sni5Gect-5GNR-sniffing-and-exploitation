#include "shadower/comp/workers/wd_worker.h"
#include "shadower/modules/dummy_exploit.h"
#include "shadower/utils/utils.h"
#include "srsran/mac/mac_rar_pdu_nr.h"
#include "srsran/mac/mac_sch_pdu_nr.h"
#include "srsran/phy/phch/pbch_msg_nr.h"
#include "srsran/phy/sync/ssb.h"
#include "srsran/phy/ue/ue_dl_nr.h"
#include "test_variables.h"
#include <fstream>

std::string ul_sample_file;
std::string last_sample_file;
uint32_t    ul_slot_number;
std::string dci_sample_file;
uint32_t    dci_slot_number;
uint32_t    half;
uint32_t    ul_offset = 0;

void parse_args(int argc, char* argv[])
{
  int opt;
  while ((opt = getopt(argc, argv, "uUdDhlo")) != -1) {
    switch (opt) {
      case 'u':
        ul_sample_file = argv[optind];
        break;
      case 'U':
        ul_slot_number = atoi(argv[optind]);
        break;
      case 'd':
        dci_sample_file = argv[optind];
        break;
      case 'D':
        dci_slot_number = atoi(argv[optind]);
        break;
      case 'h':
        half = atoi(argv[optind]);
        break;
      case 'l':
        last_sample_file = argv[optind];
        break;
      case 'o':
        ul_offset = atoi(argv[optind]);
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
  /* initialize logger */
  srslog::basic_logger& logger = srslog_init(&config);
  uint16_t              rnti   = args.c_rnti;
  switch (test_number) {
    case 0:
      ul_sample_file   = "shadower/test/data/srsran-n78-20MHz/pusch_3426.fc32";
      ul_slot_number   = 6;
      dci_sample_file  = "shadower/test/data/srsran-n78-20MHz/dci_ul_3422.fc32";
      dci_slot_number  = 2;
      last_sample_file = ul_sample_file;
      half             = 1;
      ul_offset        = 468;
      break;
    case 1:
      dci_sample_file  = "shadower/test/data/srsran-n78-40MHz/dci_ul_13622.fc32";
      dci_slot_number  = 2;
      ul_sample_file   = "shadower/test/data/srsran-n78-40MHz/pusch_13626.fc32";
      ul_slot_number   = 6;
      last_sample_file = ul_sample_file;
      half             = 1;
      ul_offset        = 768;
      break;
    case 2:
      dci_sample_file  = "shadower/test/data/effnet/dci_11686.fc32";
      dci_slot_number  = 6;
      ul_sample_file   = "shadower/test/data/effnet/dci_11688.fc32";
      ul_slot_number   = 8;
      last_sample_file = ul_sample_file;
      half             = 1;
      ul_offset        = 396;
      break;
    case 4:
      dci_sample_file  = "shadower/test/data/srsran-n3-20MHz/dci_6685.fc32";
      dci_slot_number  = 5;
      ul_sample_file   = "shadower/test/data/srsran-n3-20MHz/pusch_6689.fc32";
      ul_slot_number   = 9;
      last_sample_file = "shadower/test/data/srsran-n3-20MHz/pusch_6688.fc32";
      half             = 0;
      ul_offset        = 480;
      break;
    // case 4:
    //   dci_sample_file  = "shadower/test/data/srsran-n3-20MHz/dci_6705.fc32";
    //   dci_slot_number  = 5;
    //   ul_sample_file   = "shadower/test/data/srsran-n3-20MHz/pusch_6709.fc32";
    //   ul_slot_number   = 9;
    //   last_sample_file = "shadower/test/data/srsran-n3-20MHz/pusch_6708.fc32";
    //   half             = 0;
    //   ul_offset        = 480;
    //   break;
    // case 4:
    //   dci_sample_file  = "shadower/test/data/srsran-n3-20MHz/dci_6725.fc32";
    //   dci_slot_number  = 5;
    //   ul_sample_file   = "shadower/test/data/srsran-n3-20MHz/pusch_6729.fc32";
    //   ul_slot_number   = 9;
    //   last_sample_file = "shadower/test/data/srsran-n3-20MHz/pusch_6728.fc32";
    //   half             = 0;
    //   ul_offset        = 480;
    //   break;
    case 6:
      dci_sample_file  = "shadower/test/data/srsran-n5-10MHz/dci_10030.fc32";
      dci_slot_number  = 10;
      ul_sample_file   = "shadower/test/data/srsran-n5-10MHz/pusch_10034.fc32";
      ul_slot_number   = 14;
      last_sample_file = "shadower/test/data/srsran-n5-10MHz/pusch_10033.fc32";
      half             = 0;
      ul_offset        = 0;
      break;
    default:
      fprintf(stderr, "Unknown test number: %d\n", test_number);
      exit(EXIT_FAILURE);
  }
  /* parse command line arguments */
  parse_args(argc, argv);
  logger.info("DCI sample file: %s", dci_sample_file.c_str());
  logger.info("DCI slot number: %u", dci_slot_number);
  logger.info("Sample file: %s", ul_sample_file.c_str());
  logger.info("Slot number: %u", ul_slot_number);
  logger.info("Half: %u", half);

  /* initialize phy cfg */
  srsran::phy_cfg_nr_t phy_cfg = {};
  init_phy_cfg(phy_cfg, config);

  /* init phy state */
  srsue::nr::state phy_state = {};
  init_phy_state(phy_state, config.nof_prb);

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

  /* UE DL init with configuration from phy_cfg */
  srsran_ue_dl_nr_t ue_dl        = {};
  cf_t*             ue_dl_buffer = srsran_vec_cf_malloc(args.sf_len);
  if (!init_ue_dl(ue_dl, ue_dl_buffer, phy_cfg)) {
    logger.error("Failed to init UE DL");
    return -1;
  }

  /* GNB UL initialize with configuration from phy_cfg */
  srsran_gnb_ul_t gnb_ul        = {};
  cf_t*           gnb_ul_buffer = srsran_vec_cf_malloc(args.sf_len);
  if (!init_gnb_ul(gnb_ul, gnb_ul_buffer, phy_cfg)) {
    logger.error("Failed to init GNB UL");
    return -1;
  }

  /* load test dci samples */
  std::vector<cf_t> dci_samples(args.sf_len);
  if (!load_samples(dci_sample_file, dci_samples.data(), args.sf_len)) {
    logger.error("Failed to load data from %s", dci_sample_file.c_str());
    return -1;
  }

  /* Extract dci slot number from the file name */
  for (int i = 0; i < args.slot_per_sf; i++) {
    /* copy samples to ue_dl processing buffer */
    srsran_vec_cf_copy(ue_dl_buffer, dci_samples.data() + args.slot_len * i, args.slot_len);
    /* Initialize slot cfg */
    srsran_slot_cfg_t slot_cfg = {.idx = dci_slot_number + i};
    /* run ue_dl estimate fft */
    srsran_ue_dl_nr_estimate_fft(&ue_dl, &slot_cfg);

    char filename[64];
    sprintf(filename, "ofdm_dci_%d_fft%u", i, args.nof_sc);
    write_record_to_file(ue_dl.sf_symbols[0], args.nof_re, filename);

    std::array<srsran_dci_dl_nr_t, SRSRAN_SEARCH_SPACE_MAX_NOF_CANDIDATES_NR> dci_dl = {};
    std::array<srsran_dci_ul_nr_t, SRSRAN_SEARCH_SPACE_MAX_NOF_CANDIDATES_NR> dci_ul = {};
    /* search for dci */
    ue_dl_dci_search(ue_dl, phy_cfg, slot_cfg, rnti, srsran_rnti_type_c, phy_state, logger, 0, dci_dl, dci_ul);
  }

  /* load test samples */
  std::vector<cf_t> samples(args.sf_len);
  if (!load_samples(ul_sample_file, samples.data(), args.sf_len)) {
    logger.error("Failed to load data from %s", ul_sample_file.c_str());
    return -1;
  }

  /* load last samples from last subframe */
  std::vector<cf_t> last_samples(args.sf_len);
  if (half == 0) {
    if (!load_samples(last_sample_file, last_samples.data(), args.sf_len)) {
      logger.error("Failed to load data from %s", last_sample_file.c_str());
      return -1;
    }
  }

  /* Get the slot cfg for pusch */
  srsran_slot_cfg_t slot_cfg = {.idx = ul_slot_number + half};

  /* get uplink grant */
  uint32_t            pid       = 0;
  srsran_sch_cfg_nr_t pusch_cfg = {};
  if (!phy_state.get_ul_pending_grant(slot_cfg.idx, pusch_cfg, pid)) {
    logger.error("No uplink grant available");
    return -1;
  }

  /* copy samples to gnb_ul processing buffer */
  if (half == 0 && ul_offset > 0) {
    /* Copy the last samples to current buffer */
    srsran_vec_cf_copy(gnb_ul_buffer, last_samples.data() + args.sf_len - ul_offset, ul_offset);
    /* Copy the remaining samples from the sample file */
    srsran_vec_cf_copy(gnb_ul_buffer + ul_offset, samples.data(), args.slot_len - ul_offset);
  } else {
    /* Copy the samples to the buffer */
    srsran_vec_cf_copy(gnb_ul_buffer, samples.data() + half * args.slot_len - ul_offset, args.slot_len);
  }

  /* run gnb_ul estimate fft */
  if (srsran_gnb_ul_fft(&gnb_ul)) {
    logger.error("Error running srsran_gnb_ul_fft");
    return -1;
  }

  /* Write OFDM symbols to file for debug purpose */
  char filename[64];
  sprintf(filename, "ofdm_pusch_%u_fft%u", ul_offset, args.nof_sc);
  write_record_to_file(gnb_ul.sf_symbols[0], args.nof_re, filename);

  /* Apply the cfo to the signal with magic number */
  srsran_vec_apply_cfo(gnb_ul.sf_symbols[0], config.uplink_cfo, gnb_ul.sf_symbols[0], args.nof_re);

  /* Initialize the buffer for output*/
  srsran::unique_byte_buffer_t data = srsran::make_byte_buffer();
  if (data == nullptr) {
    logger.error("Error creating byte buffer");
    return -1;
  }
  data->N_bytes = pusch_cfg.grant.tb[0].tbs / 8U;

  /* Initialize pusch result*/
  srsran_pusch_res_nr_t pusch_res      = {};
  pusch_res.tb[0].payload              = data->msg;
  srsran_softbuffer_rx_t softbuffer_rx = {};
  if (srsran_softbuffer_rx_init_guru(&softbuffer_rx, SRSRAN_SCH_NR_MAX_NOF_CB_LDPC, SRSRAN_LDPC_MAX_LEN_ENCODED_CB) !=
      0) {
    logger.error("Couldn't allocate and/or initialize softbuffer");
    return -1;
  }

  /* Decode PUSCH */
  if (!gnb_ul_pusch_decode(gnb_ul, pusch_cfg, slot_cfg, pusch_res, softbuffer_rx, logger)) {
    logger.error("Error running gnb_ul_pusch_decode");
    return -1;
  }

  /* if the message is not decoded correctly, then return */
  if (!pusch_res.tb[0].crc) {
    logger.debug("Error PUSCH got wrong CRC");
    return -1;
  } else {
    logger.info("PUSCH CRC passed");
  }

  /* Initialize wdissector for packet summary */
  WDWorker*                        wd_worker = new WDWorker(config.duplex_mode, config.log_level);
  SafeQueue<std::vector<uint8_t> > dl_msg_queue;
  SafeQueue<std::vector<uint8_t> > ul_msg_queue;
  DummyExploit*                    exploit = new DummyExploit(dl_msg_queue, ul_msg_queue);
  wd_worker->process(data->msg, data->N_bytes, rnti, 0, 0, 0, UL, exploit);
  return 0;
}