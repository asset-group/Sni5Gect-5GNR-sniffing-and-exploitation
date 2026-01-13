#include "shadower/utils/ue_ul_utils.h"

/* ue_ul related configuration and update, ue_ul encode messages send from UE to base station*/
bool init_ue_ul(srsran_ue_ul_nr_t& ue_ul, cf_t* buffer, srsran::phy_cfg_nr_t& phy_cfg)
{
  srsran_ue_ul_nr_args_t ue_ul_args           = {};
  ue_ul_args.nof_max_prb                      = phy_cfg.carrier.nof_prb;
  ue_ul_args.pusch.sch.disable_simd           = false;
  ue_ul_args.pusch.sch.decoder_scaling_factor = 0;
  ue_ul_args.pusch.sch.decoder_use_flooded    = false;
  ue_ul_args.pusch.sch.max_nof_iter           = 10;
  ue_ul_args.pucch.max_nof_prb                = phy_cfg.carrier.nof_prb;
  ue_ul_args.pucch.uci.disable_simd           = false;
  ue_ul_args.scs                              = phy_cfg.carrier.scs;
  ue_ul_args.sample_rate_hz                   = phy_cfg.carrier.sample_rate_hz;
  if (srsran_ue_ul_nr_init(&ue_ul, buffer, &ue_ul_args) != 0) {
    return false;
  }
  if (srsran_ue_ul_nr_set_carrier(&ue_ul, &phy_cfg.carrier) != SRSRAN_SUCCESS) {
    return false;
  }
  return true;
}

/* use ue_ul to encode the message targeted to gnb */
bool update_ue_ul(srsran_ue_ul_nr_t& ue_ul, srsran::phy_cfg_nr_t& phy_cfg)
{
  if (srsran_ue_ul_nr_set_carrier(&ue_ul, &phy_cfg.carrier) != SRSRAN_SUCCESS) {
    return false;
  }
  return true;
}

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
                  uint32_t                               nof_prb_to_allocate)
{
  return false;
}