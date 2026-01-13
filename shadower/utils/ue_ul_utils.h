#pragma once
#include "srsran/common/phy_cfg_nr.h"
#include "srsran/phy/ue/ue_ul_nr.h"
#include "srsue/hdr/phy/nr/state.h"

/* ue_ul related configuration and update, ue_ul encode messages send from UE to base station*/
bool init_ue_ul(srsran_ue_ul_nr_t& ue_ul, cf_t* buffer, srsran::phy_cfg_nr_t& phy_cfg);

/* use ue_ul to encode the message targeted to gnb */
bool update_ue_ul(srsran_ue_ul_nr_t& ue_ul, srsran::phy_cfg_nr_t& phy_cfg);

/* Run PUSCH encoding for the message targeted to gnb */
bool ue_ul_encode(std::shared_ptr<std::vector<uint8_t> > msg,
                  srsran_ue_ul_nr_t&                     ue_ul,
                  srsran_dci_cfg_nr_t&                   dci_cfg,
                  srsran::phy_cfg_nr_t&                  phy_cfg,
                  srsran_sch_cfg_nr_t&                   pusch_cfg,
                  srsran_slot_cfg_t&                     slot_cfg,
                  uint16_t                               rnti,
                  srsran_rnti_type_t                     rnti_type,
                  srslog::basic_logger&                  logger,
                  uint32_t                               mcs,
                  uint32_t                               nof_prb_to_allocate);