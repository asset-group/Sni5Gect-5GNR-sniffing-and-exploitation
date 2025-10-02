#include "shadower/utils/utils.h"
#include "srsran/mac/mac_sch_pdu_nr.h"
#include "srsran/phy/phch/pbch_msg_nr.h"
#include "srsran/phy/ue/ue_dl_nr.h"
#include "test_variables.h"
#include <fstream>
#include <iomanip>
#include <sstream>

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
  std::string           sample_file;
  uint32_t              slot_number;
  uint32_t              half = 1;
  switch (test_number) {
    case 0:
      sample_file = "shadower/test/data/srsran-n78-20MHz/rrc_setup.fc32";
      slot_number = 10;
      half        = 0;
      break;
    case 1:
      sample_file = "shadower/test/data/srsran-n78-40MHz/rrc_setup.fc32";
      slot_number = 0;
      half        = 1;
      break;
    case 2:
      sample_file = "shadower/test/data/contention_resolution.fc32";
      slot_number = 16;
      half        = 0;
      break;
    case 4:
      sample_file = "shadower/test/data/srsran-n3-20MHz/contention_resolution.fc32";
      slot_number = 0;
      half        = 0;
      break;
    default:
      fprintf(stderr, "Unknown test number: %d\n", test_number);
      exit(EXIT_FAILURE);
  }

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

  /* UE DL init with configuration from phy_cfg */
  srsran_ue_dl_nr_t ue_dl  = {};
  cf_t*             buffer = srsran_vec_cf_malloc(args.sf_len);
  if (!init_ue_dl(ue_dl, buffer, phy_cfg)) {
    logger.error("Failed to init UE DL");
    return -1;
  }

  /* load test samples */
  std::vector<cf_t> samples(args.sf_len);
  if (!load_samples(sample_file, samples.data(), args.sf_len)) {
    logger.error("Failed to load data from %s", sample_file.c_str());
    return -1;
  }

  /* copy samples to ue_dl processing buffer */
  srsran_vec_cf_copy(buffer, samples.data() + args.slot_len * half, args.slot_len);
  /* Initialize slot cfg */
  srsran_slot_cfg_t slot_cfg = {.idx = slot_number + half};
  /* run ue_dl estimate fft */
  srsran_ue_dl_nr_estimate_fft(&ue_dl, &slot_cfg);

  /* Write OFDM symbols to file for debug purpose */
  char filename[64];
  sprintf(filename, "ofdm_rrc_setup_fft%u", args.nof_sc);
  write_record_to_file(ue_dl.sf_symbols[0], args.nof_re, filename);

  std::array<srsran_dci_dl_nr_t, SRSRAN_SEARCH_SPACE_MAX_NOF_CANDIDATES_NR> dci_dl = {};
  std::array<srsran_dci_ul_nr_t, SRSRAN_SEARCH_SPACE_MAX_NOF_CANDIDATES_NR> dci_ul = {};
  /* search for dci */
  ue_dl_dci_search(ue_dl, phy_cfg, slot_cfg, args.c_rnti, srsran_rnti_type_c, phy_state, logger, 0, dci_dl, dci_ul);

  /* get grant from dci search */
  uint32_t                   pid          = 0;
  srsran_sch_cfg_nr_t        pdsch_cfg    = {};
  srsran_harq_ack_resource_t ack_resource = {};
  if (!phy_state.get_dl_pending_grant(slot_cfg.idx, pdsch_cfg, ack_resource, pid)) {
    logger.error("Failed to get grant from dci search");
    return -1;
  }

  /* Initialize the buffer for output*/
  srsran::unique_byte_buffer_t data = srsran::make_byte_buffer();
  if (data == nullptr) {
    logger.error("Error creating byte buffer");
    return -1;
  }
  data->N_bytes = pdsch_cfg.grant.tb[0].tbs / 8U;

  /* Initialize pdsch result*/
  srsran_pdsch_res_nr_t pdsch_res      = {};
  pdsch_res.tb[0].payload              = data->msg;
  srsran_softbuffer_rx_t softbuffer_rx = {};
  if (srsran_softbuffer_rx_init_guru(&softbuffer_rx, SRSRAN_SCH_NR_MAX_NOF_CB_LDPC, SRSRAN_LDPC_MAX_LEN_ENCODED_CB) !=
      0) {
    logger.error("Couldn't allocate and/or initialize softbuffer");
    return -1;
  }

  /* Decode PDSCH */
  if (!ue_dl_pdsch_decode(ue_dl, pdsch_cfg, slot_cfg, pdsch_res, softbuffer_rx, logger)) {
    return -1;
  }
  /* if the message is not decoded correctly, then return */
  if (!pdsch_res.tb[0].crc) {
    logger.debug("Error PDSCH got wrong CRC");
    return -1;
  }

  /* Decode the data as mac_sch_pdu */
  srsran::mac_sch_pdu_nr pdu;
  if (pdu.unpack(data->msg, data->N_bytes) != 0) {
    logger.error("Error unpacking MAC SCH PDU.");
    return -1;
  }

  /* Iterate over all subpdus to find the dl_ccch_msg */
  uint32_t num_pdu = pdu.get_num_subpdus();
  for (uint32_t i = 0; i < num_pdu; i++) {
    srsran::mac_sch_subpdu_nr subpdu = pdu.get_subpdu(i);
    logger.info("LCID: %u length: %u", subpdu.get_lcid(), subpdu.get_sdu_length());
    if (subpdu.get_lcid() == srsran::mac_sch_subpdu_nr::CON_RES_ID) {
      srsran::mac_sch_subpdu_nr::ue_con_res_id_t contention_id = subpdu.get_ue_con_res_id_ce();
      std::ostringstream                         oss;
      for (uint32_t i = 0; i < srsran::mac_sch_subpdu_nr::ue_con_res_id_len; i++) {
        oss << std::setfill('0') << std::setw(2) << std::hex << static_cast<int>(contention_id.data()[i]);
      }
      logger.info("Contention resolution ID: %s", oss.str().c_str());
    }
  }
  return 0;
}