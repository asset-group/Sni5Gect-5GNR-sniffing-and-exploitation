#include "shadower/utils/gnb_ul_utils.h"

/* gnb_ul related configuration and update, gnb_ul decode messages send from UE to base station */
bool init_gnb_ul(srsran_gnb_ul_t& gnb_ul, cf_t* buffer, srsran::phy_cfg_nr_t& phy_cfg)
{
  srsran_gnb_ul_args_t ul_args   = {};
  ul_args.pusch.measure_time     = true;
  ul_args.pusch.measure_evm      = true;
  ul_args.pusch.max_layers       = 1;
  ul_args.pusch.sch.max_nof_iter = 10;
  ul_args.pusch.max_prb          = phy_cfg.carrier.nof_prb;
  ul_args.nof_max_prb            = phy_cfg.carrier.nof_prb;
  ul_args.pusch_min_snr_dB       = -10;
  ul_args.scs                    = phy_cfg.carrier.scs;
  ul_args.sample_rate_hz         = phy_cfg.carrier.sample_rate_hz;
  if (srsran_gnb_ul_init(&gnb_ul, buffer, &ul_args) != 0) {
    return false;
  }
  if (srsran_gnb_ul_set_carrier(&gnb_ul, &phy_cfg.carrier) != SRSRAN_SUCCESS) {
    return false;
  }
  return true;
}

bool update_gnb_ul(srsran_gnb_ul_t& gnb_ul, srsran::phy_cfg_nr_t& phy_cfg)
{
  if (srsran_gnb_ul_set_carrier(&gnb_ul, &phy_cfg.carrier) != SRSRAN_SUCCESS) {
    return false;
  }
  return true;
}

/* Detect and decode PUSCH info bytes */
bool gnb_ul_pusch_decode(srsran_gnb_ul_t&        gnb_ul,
                         srsran_sch_cfg_nr_t&    pusch_cfg,
                         srsran_slot_cfg_t&      slot_cfg,
                         srsran_pusch_res_nr_t&  pusch_res,
                         srsran_softbuffer_rx_t& softbuffer_rx,
                         srslog::basic_logger&   logger,
                         uint32_t                task_idx)
{
  /* Run pusch channel estimation */
  if (srsran_dmrs_sch_estimate(
          &gnb_ul.dmrs, &slot_cfg, &pusch_cfg, &pusch_cfg.grant, gnb_ul.sf_symbols[0], &gnb_ul.chest_pusch)) {
    logger.error("Error running srsran_dmrs_sch_estimate");
    return false;
  }
  /* if SNR is too low, return false */
  if (gnb_ul.dmrs.csi.snr_dB < gnb_ul.pusch_min_snr_dB) {
    logger.debug("SNR is too low for PUSCH decoding: slot %u", slot_cfg.idx);
    if (logger.debug.enabled()) {
      char str[256];
      srsran_gnb_ul_pusch_info(&gnb_ul, &pusch_cfg, &pusch_res, str, 256);
      logger.info("PUSCH %u %u: %s", task_idx, slot_cfg.idx, str);
    }
    return false;
  }
  /* pusch and softbuffer initialization */
  srsran_softbuffer_rx_reset(&softbuffer_rx);
  pusch_cfg.grant.tb[0].softbuffer.rx = &softbuffer_rx;
  /* pusch decoding */
  if (srsran_pusch_nr_decode(
          &gnb_ul.pusch, &pusch_cfg, &pusch_cfg.grant, &gnb_ul.chest_pusch, gnb_ul.sf_symbols, &pusch_res)) {
    logger.error("Error running srsran_pusch_nr_decode\n");
    return false;
  }
  if (logger.debug.enabled()) {
    char str[256];
    srsran_gnb_ul_pusch_info(&gnb_ul, &pusch_cfg, &pusch_res, str, 256);
    logger.debug("PUSCH %u %u: %s", task_idx, slot_cfg.idx, str);
  }
  return true;
}
