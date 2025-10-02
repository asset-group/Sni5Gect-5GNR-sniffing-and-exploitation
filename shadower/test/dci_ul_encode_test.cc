#include "shadower/comp/workers/wd_worker.h"
#include "shadower/modules/dummy_exploit.h"
#include "shadower/utils/ue_dl_utils.h"
#include "shadower/utils/utils.h"
#include "srsran/mac/mac_rar_pdu_nr.h"
#include "srsran/mac/mac_sch_pdu_nr.h"
#include "srsran/phy/phch/pbch_msg_nr.h"
#include "srsran/phy/sync/ssb.h"
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
  logger.set_level(srslog::basic_levels::debug);

  uint32_t           target_slot_idx = 7;
  uint32_t           mcs             = 3;
  uint32_t           prbs            = 12;
  uint16_t           rnti            = args.c_rnti;
  srsran_rnti_type_t rnti_type       = srsran_rnti_type_c;

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

  /* GNB DL init with configuration from phy_cfg */
  srsran_gnb_dl_t gnb_dl        = {};
  cf_t*           gnb_dl_buffer = srsran_vec_cf_malloc(args.sf_len);
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

  /* Initialize softbuffer rx */
  srsran_softbuffer_rx_t softbuffer_rx = {};
  if (srsran_softbuffer_rx_init_guru(&softbuffer_rx, SRSRAN_SCH_NR_MAX_NOF_CB_LDPC, SRSRAN_LDPC_MAX_LEN_ENCODED_CB) <
      SRSRAN_SUCCESS) {
    logger.error("Error initializing softbuffer_rx");
    return -1;
  }

  /* ##############################################
  Encode the PDCCH for DCI UL
  ###############################################*/
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
  srsran_dci_ul_nr_t dci_to_send = {};
  if (!construct_dci_ul_to_send(dci_to_send, phy_cfg, slot_cfg.idx, rnti, rnti_type, mcs, prbs)) {
    logger.error("Error constructing DCI to send");
    return -1;
  }

  /* Pack dci into pdcch */
  if (srsran_gnb_dl_pdcch_put_ul(&gnb_dl, &slot_cfg, &dci_to_send) < SRSRAN_SUCCESS) {
    logger.error("Error putting DCI into PDCCH");
    return -1;
  }

  /* Encode the message into IQ samples */
  srsran_softbuffer_tx_reset(&softbuffer_tx);
  srsran_gnb_dl_gen_signal(&gnb_dl);

  /* Write the samples to file */
  char filename[64];
  sprintf(filename, "gnb_dl_buffer_fft%u", args.nof_sc);
  write_record_to_file(gnb_dl_buffer, args.slot_len, filename);

  /* ##############################################
    After generation, verify the message can be successfully decoded
  ###############################################*/
  /* copy samples to ue_dl processing buffer */
  srsran_vec_cf_copy(ue_dl_buffer, gnb_dl_buffer, args.slot_len);

  /* run ue_dl estimate fft */
  srsran_ue_dl_nr_estimate_fft(&ue_dl, &slot_cfg);

  /* Write OFDM symbols to file for debug purpose */
  sprintf(filename, "ofdm_pdsch_fft%u", args.nof_sc);
  write_record_to_file(ue_dl.sf_symbols[0], args.nof_re, filename);

  std::array<srsran_dci_dl_nr_t, SRSRAN_SEARCH_SPACE_MAX_NOF_CANDIDATES_NR> dci_dl = {};
  std::array<srsran_dci_ul_nr_t, SRSRAN_SEARCH_SPACE_MAX_NOF_CANDIDATES_NR> dci_ul = {};
  /* search for dci */
  ue_dl_dci_search(ue_dl, phy_cfg, slot_cfg, rnti, rnti_type, phy_state, logger, 0, dci_dl, dci_ul);
  usleep(100);
  return 0;
}