#include "shadower/comp/workers/gnb_dl_worker.h"

GNBDLWorker::GNBDLWorker(srslog::basic_logger& logger_, Source* source_, ShadowerConfig& config_) :
  logger(logger_), source(source_), config(config_)
{
}

GNBDLWorker::~GNBDLWorker()
{
  if (gnb_dl_buffer) {
    free(gnb_dl_buffer);
    gnb_dl_buffer = nullptr;
  }
  if (tx_buffer) {
    free(tx_buffer);
    tx_buffer = nullptr;
  }
  if (data_tx[0]) {
    free(data_tx[0]);
    data_tx[0] = nullptr;
  }
  srsran_gnb_dl_free(&gnb_dl);
}

bool GNBDLWorker::init()
{
  std::lock_guard<std::mutex> lock(mutex);
  sf_len        = config.sample_rate * SF_DURATION;
  slot_per_sf   = 1 << config.scs_common;
  slot_len      = sf_len / slot_per_sf;
  slot_duration = SF_DURATION / slot_per_sf;
  nof_sc        = config.nof_prb * SRSRAN_NRE;
  nof_re        = nof_sc * SRSRAN_NSYMB_PER_SLOT_NR;
  numerology    = (uint32_t)config.scs_common;

  /* Init gnb_dl_buffer */
  gnb_dl_buffer = srsran_vec_cf_malloc(sf_len);
  if (!gnb_dl_buffer) {
    logger.error("Error allocating buffer");
    return false;
  }
  tx_buffer = srsran_vec_cf_malloc(sf_len * 2);
  if (!tx_buffer) {
    logger.error("Error allocating output buffer");
    return false;
  }
  cf_t* buffer_gnb[SRSRAN_MAX_PORTS] = {};
  buffer_gnb[0]                      = gnb_dl_buffer;
  /* buffer for data to send */
  data_tx[0] = srsran_vec_u8_malloc(SRSRAN_SLOT_MAX_NOF_BITS_NR);
  if (data_tx[0] == nullptr) {
    logger.error("Error allocating data buffer");
    return false;
  }

  /* Init gnb dl instance */
  srsran_gnb_dl_args_t dl_args = {};
  dl_args.pdsch.measure_time   = true;
  dl_args.pdsch.max_layers     = 1;
  dl_args.pdsch.max_prb        = config.nof_prb;
  dl_args.nof_max_prb          = config.nof_prb;
  dl_args.nof_tx_antennas      = 1;
  dl_args.srate_hz             = config.sample_rate;
  dl_args.scs                  = config.scs_common;
  /* Initialize gNB DL */
  if (srsran_gnb_dl_init(&gnb_dl, buffer_gnb, &dl_args) != SRSRAN_SUCCESS) {
    logger.error("Error initializing gNB DL");
    return false;
  }
  /* Initialize softbuffer tx */
  if (srsran_softbuffer_tx_init_guru(&softbuffer_tx, SRSRAN_SCH_NR_MAX_NOF_CB_LDPC, SRSRAN_LDPC_MAX_LEN_ENCODED_CB) <
      SRSRAN_SUCCESS) {
    logger.error("Error initializing softbuffer_tx");
    return false;
  }
  return true;
}

/* Set the carrier for the gnb_dl worker */
bool GNBDLWorker::update_cfg(srsran::phy_cfg_nr_t& phy_cfg_)
{
  std::lock_guard<std::mutex> lock(mutex);
  phy_cfg = phy_cfg_;
  if (srsran_gnb_dl_set_carrier(&gnb_dl, &phy_cfg.carrier) < SRSRAN_SUCCESS) {
    logger.error("Error setting gNB DL carrier");
    return false;
  }
  return true;
}

void GNBDLWorker::set_context(gnb_dl_task_t& task_)
{
  std::lock_guard<std::mutex> lock(mutex);
  gnb_dl_task = std::move(task_);
}

void GNBDLWorker::work_imp()
{
  std::lock_guard<std::mutex> lock(mutex);
  uint32_t                    tx_buffer_len = 0;

  /* Construct and encode the pdsch message */
  if (send_pdsch()) {
    srsran_vec_cf_copy(tx_buffer + config.front_padding, gnb_dl_buffer, slot_len);
    tx_buffer_len = config.front_padding + slot_len;
    logger.info("Attached PDSCH to slot %u", gnb_dl_task.slot_idx);
  } else {
    logger.error("Error packing message");
    return;
  }

  /* check the target slot is ul slot */
  if (srsran_duplex_nr_is_ul(&phy_cfg.duplex, numerology, gnb_dl_task.slot_idx + 1 + 4)) {
    if (send_dci_ul(gnb_dl_task.slot_idx + 1)) {
      uint32_t dci_len = 2 * (gnb_dl.fft->cfg.cp + gnb_dl.fft->cfg.symbol_sz);
      srsran_vec_cf_copy(tx_buffer + tx_buffer_len, gnb_dl_buffer, dci_len);
      tx_buffer_len += dci_len;
      logger.info("Attached DCI UL to slot %u", gnb_dl_task.slot_idx + 1);
    }
  }
  /* if the message only in one slot, then add noise to the rest of the subframe */
  if ((phy_cfg.duplex.mode == srsran_duplex_mode_t::SRSRAN_DUPLEX_MODE_TDD) &&
      tx_buffer_len == (slot_len + config.front_padding)) {
    uint32_t noise_size = slot_len * 0.8;
    srsran_vec_cf_copy(tx_buffer + tx_buffer_len, gnb_dl_buffer, noise_size);
    tx_buffer_len += noise_size;
  }

  srsran_vec_apply_cfo(tx_buffer, -config.tx_cfo_correction / config.sample_rate, tx_buffer, tx_buffer_len);
  uint32_t back_padding_size = config.back_padding;
  if (tx_buffer_len + config.back_padding > 1.8 * sf_len) {
    back_padding_size = 1.8 * sf_len - tx_buffer_len;
  }
  srsran_vec_cf_zero(tx_buffer, config.front_padding);
  srsran_vec_cf_zero(tx_buffer + tx_buffer_len, back_padding_size);
  tx_buffer_len += back_padding_size;

  /* calculate the timestamp to send out the message */
  srsran_timestamp_add(&gnb_dl_task.rx_time,
                       0,
                       slot_duration * (gnb_dl_task.slot_idx - gnb_dl_task.rx_tti) -
                           (config.tx_advancement + config.front_padding) / config.sample_rate);
  cf_t* sdr_buffer[SRSRAN_MAX_PORTS] = {};
  for (uint32_t ch = 0; ch < config.nof_channels; ch++) {
    sdr_buffer[ch] = nullptr;
  }
  sdr_buffer[0] = tx_buffer;
  source->send(sdr_buffer, tx_buffer_len, gnb_dl_task.rx_time, gnb_dl_task.slot_idx);
  logger.info("Send message to UE: RNTI %u Slot: %u Current Slot: %u, current_task_idx: %u, target_task_idx: %u",
              gnb_dl_task.rnti,
              gnb_dl_task.slot_idx,
              gnb_dl_task.rx_tti,
              gnb_dl_task.rx_task_idx,
              gnb_dl_task.rx_task_idx + (gnb_dl_task.slot_idx - gnb_dl_task.rx_tti + 1) / slot_per_sf);
}

bool GNBDLWorker::send_pdsch()
{
  if (gnb_dl_task.msg == nullptr || gnb_dl_task.msg->empty()) {
    logger.error("Message is null");
    return false;
  }

  /* clean up the resource grid first */
  if (srsran_gnb_dl_base_zero(&gnb_dl) < SRSRAN_SUCCESS) {
    logger.error("Error zero RE grid of gNB DL");
    return false;
  }

  /* Update pdcch with dci_cfg */
  srsran_dci_cfg_nr_t dci_cfg = phy_cfg.get_dci_cfg();
  if (srsran_gnb_dl_set_pdcch_config(&gnb_dl, &phy_cfg.pdcch, &dci_cfg) < SRSRAN_SUCCESS) {
    logger.error("Error setting PDCCH config for gnb dl");
    return false;
  }
  srsran_slot_cfg_t slot_cfg = {.idx = gnb_dl_task.slot_idx};

  /* Build the DCI message */
  srsran_dci_dl_nr_t dci_to_send = {};
  if (!construct_dci_dl_to_send(dci_to_send,
                                phy_cfg,
                                slot_cfg.idx,
                                gnb_dl_task.rnti,
                                gnb_dl_task.rnti_type,
                                gnb_dl_task.mcs,
                                gnb_dl_task.prbs)) {
    logger.error("Error constructing DCI to send");
    return false;
  }

  /* Pack dci into pdcch */
  if (srsran_gnb_dl_pdcch_put_dl(&gnb_dl, &slot_cfg, &dci_to_send) < SRSRAN_SUCCESS) {
    logger.error("Error putting DCI into PDCCH");
    return false;
  }

  /* pack the message to be sent */
  memset(data_tx[0], 0, SRSRAN_MAX_NRE_NR * gnb_dl_task.prbs * SRSRAN_MAX_QM);
  memcpy(data_tx[0], gnb_dl_task.msg->data(), gnb_dl_task.msg->size());

  /* get pdsch_cfg from phy_cfg */
  srsran_sch_cfg_nr_t pdsch_cfg = {};
  if (!phy_cfg.get_pdsch_cfg(slot_cfg, dci_to_send, pdsch_cfg)) {
    logger.error("Error getting PDSCH config from phy_cfg");
    return false;
  }

  pdsch_cfg.grant.tb[0].softbuffer.tx = &softbuffer_tx;
  /* put the message into pdsch */
  if (srsran_gnb_dl_pdsch_put(&gnb_dl, &slot_cfg, &pdsch_cfg, data_tx) < SRSRAN_SUCCESS) {
    logger.error("Error putting PDSCH message");
    return false;
  }

  /* Encode the message */
  srsran_softbuffer_tx_reset(&softbuffer_tx);
  srsran_gnb_dl_gen_signal(&gnb_dl);
  return true;
}

/* Construct the DCI UL message */
bool GNBDLWorker::send_dci_ul(uint32_t slot_idx)
{
  /* clean up the resource grid first */
  if (srsran_gnb_dl_base_zero(&gnb_dl) < SRSRAN_SUCCESS) {
    logger.error("Error zero RE grid of gNB DL");
    return false;
  }

  /* Update pdcch with dci_cfg */
  srsran_dci_cfg_nr_t dci_cfg = phy_cfg.get_dci_cfg();
  if (srsran_gnb_dl_set_pdcch_config(&gnb_dl, &phy_cfg.pdcch, &dci_cfg) < SRSRAN_SUCCESS) {
    logger.error("Error setting PDCCH config for gnb dl");
    return false;
  }

  /* build the DCI ul to send */
  srsran_dci_ul_nr_t dci_ul = {};
  if (!construct_dci_ul_to_send(
          dci_ul, phy_cfg, slot_idx, gnb_dl_task.rnti, gnb_dl_task.rnti_type, gnb_dl_task.mcs, gnb_dl_task.prbs)) {
    logger.error("Error constructing DCI to send");
    return false;
  }

  /* Pack dci into pdcch */
  srsran_slot_cfg_t slot_cfg = {.idx = slot_idx};
  if (srsran_gnb_dl_pdcch_put_ul(&gnb_dl, &slot_cfg, &dci_ul) < SRSRAN_SUCCESS) {
    logger.error("Error putting DCI into PDCCH");
    return false;
  }
  srsran_softbuffer_tx_reset(&softbuffer_tx);
  srsran_gnb_dl_gen_signal(&gnb_dl);
  return true;
}