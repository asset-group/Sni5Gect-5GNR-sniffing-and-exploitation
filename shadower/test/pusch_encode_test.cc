#include "shadower/test/test_variables.h"
#include "shadower/utils/utils.h"
#include "srsran/mac/mac_sch_pdu_nr.h"
#include <sstream>

uint8_t message[] = {0x01, 0x03, 0x00, 0x05, 0x00, 0x01, 0x10, 0xc0, 0x04, 0x00, 0x04, 0x34, 0x02, 0x01,
                     0x20, 0x01, 0x01, 0x34, 0x00, 0x1a, 0x61, 0xc5, 0xfa, 0x3f, 0x00, 0x00, 0x00, 0x00};

int main(int argc, char* argv[])
{
  int test_number = 0;
  if (argc > 1) {
    test_number = atoi(argv[1]);
  }
  test_args_t     args   = init_test_args(test_number);
  ShadowerConfig& config = args.config;

  // std::string dci_ul_file       = "shadower/test/data/srsran-n78-20MHz/dci_ul_3422.fc32";
  // uint32_t    dci_slot_number   = 2;
  // uint32_t    pusch_slot_number = 3426;
  std::string dci_ul_file       = "shadower/test/data/srsran-n5-10MHz/dci_10030.fc32";
  uint32_t    dci_slot_number   = 10;
  uint32_t    pusch_slot_number = 14;

  /* initialize logger */
  srslog::basic_logger& logger = srslog_init(&args.config);
  logger.set_level(srslog::basic_levels::debug);

  uint16_t           rnti      = args.c_rnti;
  srsran_rnti_type_t rnti_type = srsran_rnti_type_c;

  /* initialize phy cfg */
  srsran::phy_cfg_nr_t phy_cfg = {};
  init_phy_cfg(phy_cfg, config);

  /* init phy state */
  srsue::nr::state phy_state = {};
  init_phy_state(phy_state, config.nof_prb);

  /* load mib configuration and update phy_cfg */
  if (!configure_phy_cfg_from_mib(phy_cfg, args.mib_config_raw, args.ncellid)) {
    printf("Failed to configure phy cfg from mib\n");
    return -1;
  }

  /* load sib1 configuration and apply to phy_cfg */
  if (!configure_phy_cfg_from_sib1(phy_cfg, args.sib_config_raw, args.sib_size)) {
    logger.error("Failed to configure phy cfg from sib1");
    return -1;
  }

  /* load rrc_setup cell configuration and apply to phy_cfg */
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

  /* UE UL init with configuration from phy_cfg */
  srsran_ue_ul_nr_t ue_ul        = {};
  cf_t*             ue_ul_buffer = srsran_vec_cf_malloc(args.sf_len);
  if (!init_ue_ul(ue_ul, ue_ul_buffer, phy_cfg)) {
    logger.error("Failed to init UE UL");
    return -1;
  }

  /* GNB UL initialize with configuration from phy_cfg */
  srsran_gnb_ul_t gnb_ul        = {};
  cf_t*           gnb_ul_buffer = srsran_vec_cf_malloc(args.sf_len);
  if (!init_gnb_ul(gnb_ul, gnb_ul_buffer, phy_cfg)) {
    logger.error("Failed to init GNB UL");
    return -1;
  }

  /* ########################################### Get the DCI UL grant ########################################### */
  std::vector<cf_t> dci_samples(args.sf_len);
  if (!load_samples(dci_ul_file, dci_samples.data(), args.sf_len)) {
    logger.error("Failed to load data from %s", dci_ul_file.c_str());
    return -1;
  }
  for (int i = 0; i < args.slot_per_sf; i++) {
    /* copy samples to ue_dl processing buffer */
    srsran_vec_cf_copy(ue_dl_buffer, dci_samples.data() + args.slot_len * i, args.slot_len);
    /* Initialize slot cfg */
    srsran_slot_cfg_t slot_cfg = {.idx = dci_slot_number + i};
    /* run ue_dl estimate fft */
    srsran_ue_dl_nr_estimate_fft(&ue_dl, &slot_cfg);

    std::array<srsran_dci_dl_nr_t, SRSRAN_SEARCH_SPACE_MAX_NOF_CANDIDATES_NR> dci_dl = {};
    std::array<srsran_dci_ul_nr_t, SRSRAN_SEARCH_SPACE_MAX_NOF_CANDIDATES_NR> dci_ul = {};
    /* search for dci */
    ue_dl_dci_search(ue_dl, phy_cfg, slot_cfg, rnti, rnti_type, phy_state, logger, 0, dci_dl, dci_ul);
  }

  /* ########################################## Encode the PUSCH message ########################################## */
  uint32_t            pid         = 0;
  srsran_sch_cfg_nr_t pusch_cfg   = {};
  srsran_slot_cfg_t   ul_slot_cfg = {.idx = pusch_slot_number};

  // Get pending ul grant
  bool has_pusch_grant = phy_state.get_ul_pending_grant(ul_slot_cfg.idx, pusch_cfg, pid);
  if (!has_pusch_grant) {
    logger.error("No UL grant available at slot %d", ul_slot_cfg.idx);
    return -1;
  }

  // Setup frequency offset
  srsran_ue_ul_nr_set_freq_offset(&ue_ul, phy_state.get_ul_cfo());

  // Initialize softbuffer tx
  srsran_softbuffer_tx_t softbuffer_tx = {};
  if (srsran_softbuffer_tx_init_guru(&softbuffer_tx, SRSRAN_SCH_NR_MAX_NOF_CB_LDPC, SRSRAN_LDPC_MAX_LEN_ENCODED_CB) <
      SRSRAN_SUCCESS) {
    logger.error("Error initializing softbuffer_tx");
    return -1;
  }
  pusch_cfg.grant.tb->softbuffer.tx = &softbuffer_tx;

  // Initialize PUSCH data
  srsran_pusch_data_nr_t pusch_data = {};
  pusch_data.payload[0]             = srsran_vec_u8_malloc(pusch_cfg.grant.tb->nof_bits / 8);
  memset(pusch_data.payload[0], 0, pusch_cfg.grant.tb->nof_bits / 8);
  memcpy(pusch_data.payload[0], message, sizeof(message));

  srsran_dci_ul_nr_t dci_ul     = {};
  dci_ul.freq_domain_assignment = 1176;
  dci_ul.time_domain_assignment = 0;
  dci_ul.mcs                    = 9;
  dci_ul.ndi                    = 1;
  dci_ul.tpc                    = 1;
  dci_ul.dai1                   = 3;
  dci_ul.ports                  = 2;
  dci_ul.ulsch                  = 1;

  if (!phy_cfg.get_pusch_cfg(ul_slot_cfg, dci_ul, pusch_cfg)) {
    logger.error("Failed to get default pusch cfg from phy_cfg");
    return -1;
  }

  if (srsran_ue_ul_nr_encode_pusch(&ue_ul, &ul_slot_cfg, &pusch_cfg, &pusch_data) < SRSRAN_SUCCESS) {
    logger.error("Failed to encode PUSCH");
    return -1;
  }
  srsran_uci_data_nr_t uci_data = {};

  char filename[64];
  sprintf(filename, "ue_ul_buffer");
  write_record_to_file(ue_ul_buffer, args.slot_len, filename);

  /* ########################################## Decode the PUSCH message ########################################## */
  /* Initialize softbuffer rx */
  srsran_softbuffer_rx_t softbuffer_rx = {};
  if (srsran_softbuffer_rx_init_guru(&softbuffer_rx, SRSRAN_SCH_NR_MAX_NOF_CB_LDPC, SRSRAN_LDPC_MAX_LEN_ENCODED_CB) <
      SRSRAN_SUCCESS) {
    logger.error("Error initializing softbuffer_rx");
    return -1;
  }

  srsran_vec_cf_copy(gnb_ul_buffer, ue_ul_buffer, args.slot_len);
  if (srsran_gnb_ul_fft(&gnb_ul)) {
    logger.error("Error running srsran_gnb_ul_fft");
    return -1;
  }

  srsran::unique_byte_buffer_t decoded_data = srsran::make_byte_buffer();
  if (decoded_data == nullptr) {
    logger.error("Error creating byte buffer");
    return -1;
  }
  decoded_data->N_bytes           = pusch_cfg.grant.tb[0].tbs / 8U;
  srsran_pusch_res_nr_t pusch_res = {};
  pusch_res.tb[0].payload         = decoded_data->data();

  /* Decoded PUSCH */
  if (!gnb_ul_pusch_decode(gnb_ul, pusch_cfg, ul_slot_cfg, pusch_res, softbuffer_rx, logger, 0)) {
    logger.error("Failed to decode PUSCH");
    return -1;
  }
  if (!pusch_res.tb[0].crc) {
    logger.error("PUSCH CRC failed");
    return -1;
  } else {
    logger.info("PUSCH CRC passed");
  }

  std::stringstream ss;
  for (uint32_t i = 0; i < pusch_cfg.grant.tb[0].tbs / 8U; i++) {
    ss << std::hex << std::setw(2) << std::setfill('0') << (int)pusch_res.tb[0].payload[i] << " ";
  }
  logger.info("PUSCH payload: %s", ss.str().c_str());

  std::stringstream ss_exp;
  for (uint32_t i = 0; i < sizeof(message); i++) {
    ss_exp << std::hex << std::setw(2) << std::setfill('0') << (int)message[i] << " ";
  }
  logger.info("Expected payload: %s", ss_exp.str().c_str());
}