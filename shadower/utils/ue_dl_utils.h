#pragma once
#include "srsran/common/phy_cfg_nr.h"
#include "srsran/phy/ue/ue_dl_nr.h"
#include "srsue/hdr/phy/nr/state.h"

/* ue_dl related configuration and update, ue_dl decode messages send from base station to UE*/
bool init_ue_dl(srsran_ue_dl_nr_t& ue_dl, cf_t* buffer, srsran::phy_cfg_nr_t& phy_cfg);

bool update_ue_dl(srsran_ue_dl_nr_t& ue_dl, srsran::phy_cfg_nr_t& phy_cfg);

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
                      std::array<srsran_dci_ul_nr_t, SRSRAN_SEARCH_SPACE_MAX_NOF_CANDIDATES_NR>& dci_ul);

/* Detect and decode PDSCH info bytes */
bool ue_dl_pdsch_decode(srsran_ue_dl_nr_t&      ue_dl,
                        srsran_sch_cfg_nr_t&    pdsch_cfg,
                        srsran_slot_cfg_t&      slot_cfg,
                        srsran_pdsch_res_nr_t&  pdsch_res,
                        srsran_softbuffer_rx_t& softbuffer_rx,
                        srslog::basic_logger&   logger,
                        uint32_t                task_idx = 0);