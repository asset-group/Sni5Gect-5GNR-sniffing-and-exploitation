#include "shadower/utils/gnb_dl_utils.h"
#include "shadower/utils/dci_utils.h"

/* gnb_dl related configuration and update, gnb_dl encode messages send from base station to UE */
bool init_gnb_dl(srsran_gnb_dl_t& gnb_dl, cf_t* buffer, srsran::phy_cfg_nr_t& phy_cfg, double srate)
{
  srsran_gnb_dl_args_t dl_args = {};
  dl_args.pdsch.measure_time   = true;
  dl_args.pdsch.max_layers     = 1;
  dl_args.pdsch.max_prb        = phy_cfg.carrier.nof_prb;
  dl_args.nof_max_prb          = phy_cfg.carrier.nof_prb;
  dl_args.nof_tx_antennas      = 1;
  dl_args.srate_hz             = srate;
  dl_args.scs                  = phy_cfg.carrier.scs;
  dl_args.srate_hz             = phy_cfg.carrier.sample_rate_hz;

  std::array<cf_t*, SRSRAN_MAX_PORTS> tx_buffer = {};
  tx_buffer[0]                                  = buffer;
  if (srsran_gnb_dl_init(&gnb_dl, tx_buffer.data(), &dl_args) != 0) {
    return false;
  }
  if (srsran_gnb_dl_set_carrier(&gnb_dl, &phy_cfg.carrier) != SRSRAN_SUCCESS) {
    return false;
  }
  return true;
}

bool update_gnb_dl(srsran_gnb_dl_t& gnb_dl, srsran::phy_cfg_nr_t& phy_cfg)
{
  if (srsran_gnb_dl_set_carrier(&gnb_dl, &phy_cfg.carrier) != SRSRAN_SUCCESS) {
    return false;
  }
  return true;
}

/* use gnb_dl to encode the message targeted to UE */
bool gnb_dl_encode(std::shared_ptr<std::vector<uint8_t> > msg,
                   srsran_gnb_dl_t&                       gnb_dl,
                   srsran_dci_cfg_nr_t&                   dci_cfg,
                   srsran::phy_cfg_nr_t&                  phy_cfg,
                   srsran_sch_cfg_nr_t&                   pdsch_cfg,
                   srsran_slot_cfg_t&                     slot_cfg,
                   uint16_t                               rnti,
                   srsran_rnti_type_t                     rnti_type,
                   srslog::basic_logger&                  logger,
                   uint32_t                               mcs,
                   uint32_t                               nof_prb_to_allocate)
{
  /* set RE grid to zero */
  if (srsran_gnb_dl_base_zero(&gnb_dl) < SRSRAN_SUCCESS) {
    logger.error("Error initialize gnb_dl by running srsran_gnb_dl_base_zero");
    return false;
  }
  /* update pdcch with dci_cfg */
  dci_cfg = phy_cfg.get_dci_cfg();
  if (srsran_gnb_dl_set_pdcch_config(&gnb_dl, &phy_cfg.pdcch, &dci_cfg) < SRSRAN_SUCCESS) {
    logger.error("Error set pdcch config");
    return false;
  }
  /* Build the DCI message */
  srsran_dci_dl_nr_t dci_to_send = {};
  if (!construct_dci_dl_to_send(dci_to_send, phy_cfg, slot_cfg.idx, rnti, rnti_type, mcs, nof_prb_to_allocate)) {
    logger.error("Error construct dci to send");
    return false;
  }
  /* Pack dci into pdcch */
  if (srsran_gnb_dl_pdcch_put_dl(&gnb_dl, &slot_cfg, &dci_to_send) < SRSRAN_SUCCESS) {
    logger.error("Error put dci into pdcch");
    return false;
  }
  /* pack the message to be sent */
  uint8_t* data_to_send[SRSRAN_MAX_TB] = {};
  uint8_t  payload[SRSRAN_SLOT_MAX_NOF_BITS_NR];
  memset(payload, 0, SRSRAN_SLOT_MAX_NOF_BITS_NR);
  data_to_send[0] = payload;
  memcpy(payload, msg->data(), msg->size());

  /* get pdsch cfg from phy_cfg */
  if (!phy_cfg.get_pdsch_cfg(slot_cfg, dci_to_send, pdsch_cfg)) {
    logger.error("Error get pdsch cfg");
    return false;
  }
  srsran_softbuffer_tx_t softbuffer_tx = {};
  if (srsran_softbuffer_tx_init_guru(&softbuffer_tx, SRSRAN_SCH_NR_MAX_NOF_CB_LDPC, SRSRAN_LDPC_MAX_LEN_ENCODED_CB) <
      SRSRAN_SUCCESS) {
    logger.error("Error initializing softbuffer");
    return false;
  }
  pdsch_cfg.grant.tb[0].softbuffer.tx = &softbuffer_tx;
  if (srsran_gnb_dl_pdsch_put(&gnb_dl, &slot_cfg, &pdsch_cfg, data_to_send) < SRSRAN_SUCCESS) {
    logger.error("Error putting PDSCH message");
    return false;
  }

  /* generate the actual signal */
  srsran_gnb_dl_gen_signal(&gnb_dl);
  return true;
}