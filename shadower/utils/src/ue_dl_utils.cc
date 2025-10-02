#include "shadower/utils/ue_dl_utils.h"

/* ue_dl related configuration and update, ue_dl decode messages send from base station to UE*/
bool init_ue_dl(srsran_ue_dl_nr_t& ue_dl, cf_t* buffer, srsran::phy_cfg_nr_t& phy_cfg)
{
  srsran_ue_dl_nr_args_t ue_dl_args             = {};
  ue_dl_args.nof_max_prb                        = phy_cfg.carrier.nof_prb;
  ue_dl_args.nof_rx_antennas                    = 1;
  ue_dl_args.pdcch.measure_evm                  = false;
  ue_dl_args.pdcch.measure_time                 = false;
  ue_dl_args.pdcch.disable_simd                 = false;
  ue_dl_args.pdsch.sch.disable_simd             = false;
  ue_dl_args.pdsch.sch.decoder_use_flooded      = false;
  ue_dl_args.pdsch.sch.decoder_scaling_factor   = 0;
  ue_dl_args.pdsch.sch.max_nof_iter             = 10;
  ue_dl_args.scs                                = phy_cfg.carrier.scs;
  ue_dl_args.sample_rate_hz                     = phy_cfg.carrier.sample_rate_hz;
  std::array<cf_t*, SRSRAN_MAX_PORTS> rx_buffer = {};
  rx_buffer[0]                                  = buffer;
  if (srsran_ue_dl_nr_init(&ue_dl, rx_buffer.data(), &ue_dl_args) != 0) {
    return false;
  }
  if (!update_ue_dl(ue_dl, phy_cfg)) {
    return false;
  }
  return true;
}

bool update_ue_dl(srsran_ue_dl_nr_t& ue_dl, srsran::phy_cfg_nr_t& phy_cfg)
{
  if (srsran_ue_dl_nr_set_carrier(&ue_dl, &phy_cfg.carrier) != SRSRAN_SUCCESS) {
    return false;
  }
  srsran_dci_cfg_nr_t dci_cfg = phy_cfg.get_dci_cfg();
  if (srsran_ue_dl_nr_set_pdcch_config(&ue_dl, &phy_cfg.pdcch, &dci_cfg) != SRSRAN_SUCCESS) {
    return false;
  }
  return true;
}

/* Run PDCCH search for every CORESET and detect DCI for both dl and ul */
void ue_dl_dci_search(srsran_ue_dl_nr_t&                                                         ue_dl,
                      srsran::phy_cfg_nr_t&                                                      phy_cfg,
                      srsran_slot_cfg_t&                                                         slot_cfg,
                      uint16_t                                                                   rnti,
                      srsran_rnti_type_t                                                         rnti_type,
                      srsue::nr::state&                                                          phy_state,
                      srslog::basic_logger&                                                      logger,
                      uint32_t                                                                   task_idx,
                      std::array<srsran_dci_dl_nr_t, SRSRAN_SEARCH_SPACE_MAX_NOF_CANDIDATES_NR>& dci_dl,
                      std::array<srsran_dci_ul_nr_t, SRSRAN_SEARCH_SPACE_MAX_NOF_CANDIDATES_NR>& dci_ul)

{
  char dci_str[256];
  ue_dl.num_dl_dci = 0;
  ue_dl.num_ul_dci = 0;
  /* Estimate PDCCH channel for every configured CORESET for each slot */
  for (uint32_t i = 0; i < SRSRAN_UE_DL_NR_MAX_NOF_CORESET; i++) {
    if (ue_dl.cfg.coreset_present[i]) {
      srsran_dmrs_pdcch_estimate(&ue_dl.dmrs_pdcch[i], &slot_cfg, ue_dl.sf_symbols[0]);
    }
  }
  /* Function used to detect the DCI for DL within the slot*/
  int num_dci_dl =
      srsran_ue_dl_nr_find_dl_dci(&ue_dl, &slot_cfg, rnti, rnti_type, dci_dl.data(), (uint32_t)dci_dl.size());
  ue_dl.num_dl_dci = num_dci_dl;
  for (int i = 0; i < num_dci_dl; i++) {
    phy_state.set_dl_pending_grant(phy_cfg, slot_cfg, dci_dl[i]);
    if (logger.debug.enabled()) {
      srsran_dci_dl_nr_to_str(&ue_dl.dci, &dci_dl[i], dci_str, 256);
      logger.debug("DCI DL slot %u %u: %s", task_idx, slot_cfg.idx, dci_str);
    }
  }
  /* Function used to detect the DCI for UL within the slot*/
  int num_dci_ul =
      srsran_ue_dl_nr_find_ul_dci(&ue_dl, &slot_cfg, rnti, rnti_type, dci_ul.data(), (uint32_t)dci_ul.size());
  ue_dl.num_ul_dci = num_dci_ul;
  for (int i = 0; i < num_dci_ul; i++) {
    phy_state.set_ul_pending_grant(phy_cfg, slot_cfg, dci_ul[i]);
    if (logger.debug.enabled()) {
      srsran_dci_ul_nr_to_str(&ue_dl.dci, &dci_ul[i], dci_str, 256);
      logger.debug("DCI UL slot %u %u: %s", task_idx, slot_cfg.idx, dci_str);
    }
  }
}

/* Detect and decode PDSCH info bytes */
bool ue_dl_pdsch_decode(srsran_ue_dl_nr_t&      ue_dl,
                        srsran_sch_cfg_nr_t&    pdsch_cfg,
                        srsran_slot_cfg_t&      slot_cfg,
                        srsran_pdsch_res_nr_t&  pdsch_res,
                        srsran_softbuffer_rx_t& softbuffer_rx,
                        srslog::basic_logger&   logger,
                        uint32_t                task_idx)
{
  /* Initialize softbuffer */
  srsran_softbuffer_rx_reset(&softbuffer_rx);
  pdsch_cfg.grant.tb[0].softbuffer.rx = &softbuffer_rx;

  /* call srsran API to decode pdsch message */
  if (srsran_ue_dl_nr_decode_pdsch(&ue_dl, &slot_cfg, &pdsch_cfg, &pdsch_res) != 0) {
    logger.error("Error srsran_ue_dl_nr_decode_pdsch");
    return false;
  }

  if (logger.debug.enabled()) {
    char str[256];
    srsran_ue_dl_nr_pdsch_info(&ue_dl, &pdsch_cfg, &pdsch_res, str, 256);
    logger.debug("PDSCH %u %u: %s", task_idx, slot_cfg.idx, str);
  }
  return true;
}
