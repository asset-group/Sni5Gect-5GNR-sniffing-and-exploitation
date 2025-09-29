#include "shadower/comp/workers/ue_ul_worker.h"
#include "shadower/source/source.h"
#include "shadower/test/test_variables.h"
#include "shadower/utils/utils.h"

int main(int argc, char* argv[])
{
  int test_number = 0;
  if (argc > 1) {
    test_number = atoi(argv[1]);
  }
  test_args_t     args         = init_test_args(0);
  ShadowerConfig& config       = args.config;
  config.log_level             = srslog::basic_levels::debug;
  srslog::basic_logger& logger = srslog_init(&config);

  std::string dci_sample_file;
  uint32_t    dci_slot_number = 0;
  uint32_t    half            = 0;
  switch (test_number) {
    case 0:
      dci_sample_file = "shadower/test/data/srsran-n78-20MHz/dci_ul_3422.fc32";
      dci_slot_number = 2;
      half            = 1;
      break;
    default:
      fprintf(stderr, "Unknown test number: %d\n", test_number);
      return -1;
  }

  /* Initialize phy cfg */
  srsran::phy_cfg_nr_t phy_cfg = {};
  init_phy_cfg(phy_cfg, config);

  if (!configure_phy_cfg_from_mib(phy_cfg, args.mib_config_raw, args.ncellid)) {
    logger.error("Failed to configure phy cfg from mib");
    return -1;
  }

  if (!configure_phy_cfg_from_sib1(phy_cfg, args.sib_config_raw, args.sib_size)) {
    logger.error("Failed to configure phy cfg from SIB1");
    return -1;
  }

  if (!configure_phy_cfg_from_rrc_setup(phy_cfg, args.rrc_setup_raw, args.rrc_setup_size, logger)) {
    logger.error("Failed to configure phy cfg from RRC setup");
    return -1;
  }

  /* init phy state */
  srsue::nr::state phy_state = {};
  init_phy_state(phy_state, config.nof_prb);

  /* Initialize ue dl to search for DCI */
  srsran_ue_dl_nr_t ue_dl  = {};
  cf_t*             buffer = srsran_vec_cf_malloc(args.sf_len);
  if (!init_ue_dl(ue_dl, buffer, phy_cfg)) {
    logger.error("Failed to init UE DL");
    return -1;
  }

  /* load DCI samples */
  std::vector<cf_t> dci_samples(args.sf_len);
  if (!load_samples(dci_sample_file, dci_samples.data(), args.sf_len)) {
    logger.error("Failed to load DCI samples");
    return -1;
  }

  /* copy samples to ue_ul processing buffer */
  srsran_vec_cf_copy(buffer, dci_samples.data() + half * args.slot_len, args.slot_len);
  srsran_slot_cfg_t dci_slot_cfg = {.idx = dci_slot_number + half};

  /* Process the DCI sample file */
  srsran_ue_dl_nr_estimate_fft(&ue_dl, &dci_slot_cfg);

  /* search for dci */
  ue_dl.num_dl_dci = 0;
  ue_dl.num_ul_dci = 0;

  /* Estimate PDCCH channel for every configured CORESET for each slot */
  for (uint32_t i = 0; i < SRSRAN_UE_DL_NR_MAX_NOF_CORESET; i++) {
    if (ue_dl.cfg.coreset_present[i]) {
      srsran_dmrs_pdcch_estimate(&ue_dl.dmrs_pdcch[i], &dci_slot_cfg, ue_dl.sf_symbols[0]);
    }
  }

  /* Function used to detect the DCI for DL within the slot*/
  std::array<srsran_dci_dl_nr_t, SRSRAN_SEARCH_SPACE_MAX_NOF_CANDIDATES_NR> dci_dl = {};
  int                                                                       num_dci_dl =
      srsran_ue_dl_nr_find_dl_dci(&ue_dl, &dci_slot_cfg, args.c_rnti, srsran_rnti_type_c, dci_dl.data(), dci_dl.size());
  ue_dl.num_dl_dci = num_dci_dl;

  /* Function used to detect the DCI for UL within the slot*/
  std::array<srsran_dci_ul_nr_t, SRSRAN_SEARCH_SPACE_MAX_NOF_CANDIDATES_NR> dci_ul = {};
  int                                                                       num_dci_ul =
      srsran_ue_dl_nr_find_ul_dci(&ue_dl, &dci_slot_cfg, args.c_rnti, srsran_rnti_type_c, dci_ul.data(), dci_ul.size());
  ue_dl.num_ul_dci = num_dci_ul;

  if (ue_dl.num_ul_dci == 0) {
    logger.error("No UL DCI found");
    return -1;
  }
  char dci_str[256];
  for (int i = 0; i < ue_dl.num_ul_dci; i++) {
    srsran_dci_ul_nr_to_str(&ue_dl.dci, &dci_ul[i], dci_str, 256);
    logger.info("UL DCI found: %s", dci_str);
  }

  /* Initialize ue ul worker */
  create_source_t creator = load_source(file_source_module_path);
  config.source_params    = "/dev/random";
  Source* source          = creator(config);

  UEULWorker* ue_ul_worker = new UEULWorker(logger, config, source, phy_state);
  ue_ul_worker->init(phy_cfg);
  ue_ul_worker->update_cfg(phy_cfg);
  uint32_t k = ue_ul_worker->set_pusch_grant(dci_ul[0], dci_slot_cfg);

  std::shared_ptr<std::vector<uint8_t> > pusch_payload = std::make_shared<std::vector<uint8_t> >(256, 0);
  for (size_t i = 0; i < 256; i++) {
    (*pusch_payload)[i] = i;
  }

  srsran_timestamp_t rx_timestamp = {};
  uint32_t           tx_slot      = TTI_ADD(dci_slot_cfg.idx, k);
  srsran_slot_cfg_t  tx_slot_cfg  = {.idx = tx_slot};
  ue_ul_worker->send_pusch(tx_slot_cfg, pusch_payload, tx_slot, rx_timestamp, dci_ul[0]);

  /* Initialize gnb_ul to try to decode the generated messages */
  srsran_gnb_ul_t gnb_ul        = {};
  cf_t*           gnb_ul_buffer = srsran_vec_cf_malloc(args.sf_len);
  if (!init_gnb_ul(gnb_ul, gnb_ul_buffer, phy_cfg)) {
    logger.error("Failed to init gnb_ul");
    return -1;
  }
  srsran_vec_cf_copy(gnb_ul_buffer, ue_ul_worker->buffer, args.slot_len);
  /* run gnb_ul estimate fft */
  if (srsran_gnb_ul_fft(&gnb_ul)) {
    logger.error("Error running srsran_gnb_ul_fft");
    return -1;
  }

  /* Set pusch grant for decoding */
  phy_state.set_ul_pending_grant(phy_cfg, dci_slot_cfg, dci_ul[0]);
  uint32_t            pid             = 0;
  srsran_sch_cfg_nr_t pusch_cfg       = {};
  bool                has_pusch_grant = phy_state.get_ul_pending_grant(tx_slot_cfg.idx, pusch_cfg, pid);
  if (!has_pusch_grant) {
    logger.info("No PUSCH grant available");
  }

  /* Initialize the buffer for output*/
  srsran::unique_byte_buffer_t data = srsran::make_byte_buffer();
  if (data == nullptr) {
    logger.error("Error creating byte buffer");
    return -1;
  }
  data->N_bytes = pusch_cfg.grant.tb[0].tbs / 8U;

  srsran_pusch_res_nr_t pusch_res      = {};
  pusch_res.tb[0].payload              = data->msg;
  srsran_softbuffer_rx_t softbuffer_rx = {};

  if (srsran_softbuffer_rx_init_guru(&softbuffer_rx, SRSRAN_SCH_NR_MAX_NOF_CB_LDPC, SRSRAN_LDPC_MAX_LEN_ENCODED_CB) !=
      0) {
    logger.error("Couldn't allocate and/or initialize softbuffer");
    return -1;
  }
  /* Decode PUSCH */
  if (!gnb_ul_pusch_decode(gnb_ul, pusch_cfg, tx_slot_cfg, pusch_res, softbuffer_rx, logger)) {
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
}