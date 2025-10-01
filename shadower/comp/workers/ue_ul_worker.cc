#include "shadower/comp/workers/ue_ul_worker.h"

UEULWorker::UEULWorker(srslog::basic_logger& logger_,
                       ShadowerConfig&       config_,
                       Source*               source_,
                       srsue::nr::state&     phy_state_) :
  logger(logger_), config(config_), phy_state(phy_state_), source(source_), srsran::thread("UEULWorker")
{
}

UEULWorker::~UEULWorker()
{
  if (buffer) {
    free(buffer);
    buffer = nullptr;
  }
  srsran_ue_ul_nr_free(&ue_ul);
}

/* Initialize the UE UL worker  */
bool UEULWorker::init(srsran::phy_cfg_nr_t& phy_cfg_)
{
  std::lock_guard<std::mutex> lock(mutex);
  phy_cfg       = phy_cfg_;
  srate         = config.sample_rate;
  sf_len        = srate * SF_DURATION;
  slot_per_sf   = 1 << config.scs_common;
  slot_len      = sf_len / slot_per_sf;
  nof_sc        = config.nof_prb * SRSRAN_NRE;
  nof_re        = nof_sc * SRSRAN_NSYMB_PER_SLOT_NR;
  slot_duration = SF_DURATION / slot_per_sf;

  /* Init buffer */
  buffer = srsran_vec_cf_malloc(sf_len);
  if (!buffer) {
    logger.error("Error allocating buffer");
    return false;
  }

  /* Init ue_ul instance */
  if (!init_ue_ul(ue_ul, buffer, phy_cfg)) {
    logger.error("Error initializing ue_ul");
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

/* Update the UE UL configurations */
bool UEULWorker::update_cfg(srsran::phy_cfg_nr_t& phy_cfg_)
{
  std::lock_guard<std::mutex> lock(mutex);
  phy_cfg = phy_cfg_;
  if (!update_ue_ul(ue_ul, phy_cfg)) {
    logger.error("Failed to update ue_ul with new phy_cfg");
    return false;
  }
  return true;
}

/* Set the context of the UE UL worker */
void UEULWorker::set_context(std::shared_ptr<ue_ul_task_t> task_)
{
  std::lock_guard<std::mutex> lock(mutex);
  current_task = task_;
}

/* Set PUSCH grant */
int UEULWorker::set_pusch_grant(srsran_dci_ul_nr_t& dci_ul, srsran_slot_cfg_t& slot_cfg)
{
  phy_state.set_ul_pending_grant(phy_cfg, slot_cfg, dci_ul);
  std::lock_guard<std::mutex> lock(mutex);

  srsran_sch_cfg_nr_t pusch_cfg = {};
  if (not phy_cfg.get_pusch_cfg(slot_cfg, dci_ul, pusch_cfg)) {
    logger.error("Error computing PUSCH configuration");
    return -1;
  }
  target_slot.idx = TTI_ADD(slot_cfg.idx, pusch_cfg.grant.k);
  target_dci      = dci_ul;
  grant_available = true;
  cv.notify_one();
  return target_slot.idx;
}

void UEULWorker::send_pusch(srsran_slot_cfg_t&                      slot_cfg,
                            std::shared_ptr<std::vector<uint8_t> >& pusch_payload,
                            uint32_t                                rx_slot_idx,
                            srsran_timestamp_t&                     rx_timestamp,
                            srsran_dci_ul_nr_t&                     dci_ul)
{
  uint32_t            pid             = 0;
  srsran_sch_cfg_nr_t pusch_cfg       = {};
  bool                has_pusch_grant = phy_state.get_ul_pending_grant(slot_cfg.idx, pusch_cfg, pid);
  if (!has_pusch_grant) {
    logger.error("No UL grant available at slot %d", slot_cfg.idx);
    return;
  }

  // Setup frequency offset
  srsran_ue_ul_nr_set_freq_offset(&ue_ul, phy_state.get_ul_cfo());
  pusch_cfg.grant.tb->softbuffer.tx = &softbuffer_tx;
  srsran_softbuffer_tx_reset(&softbuffer_tx);

  // Initialize PUSCH data
  srsran_pusch_data_nr_t pusch_data      = {};
  uint32_t               number_of_bytes = pusch_cfg.grant.tb->nof_bits / 8;
  pusch_data.payload[0]                  = srsran_vec_u8_malloc(number_of_bytes);
  memcpy(pusch_data.payload[0], pusch_payload->data(), number_of_bytes);

  // Get PUSCH configuration from dci_ul
  if (!phy_cfg.get_pusch_cfg(slot_cfg, dci_ul, pusch_cfg)) {
    logger.error("Failed to get PUSCH configuration");
    free(pusch_data.payload[0]);
    return;
  }

  // encode PUSCH
  if (srsran_ue_ul_nr_encode_pusch(&ue_ul, &slot_cfg, &pusch_cfg, &pusch_data) != SRSRAN_SUCCESS) {
    logger.error("Failed to encode PUSCH");
    free(pusch_data.payload[0]);
    return;
  }
  logger.info("Encoded PUSCH for slot %u", slot_cfg.idx);

  // Calculate the timestamp to send out the message
  srsran_timestamp_t tx_timestamp = {};
  srsran_timestamp_copy(&tx_timestamp, &rx_timestamp);
  srsran_timestamp_add(&tx_timestamp,
                       0,
                       (slot_cfg.idx - rx_slot_idx) * slot_duration -
                           (config.ul_advancement + config.front_padding) / config.sample_rate);
  cf_t* sdr_buffer[SRSRAN_MAX_PORTS] = {};
  for (uint32_t ch = 0; ch < config.nof_channels; ch++) {
    sdr_buffer[ch] = nullptr;
  }
  sdr_buffer[config.ul_channel] = buffer;
  source->send(sdr_buffer, slot_len, tx_timestamp, slot_cfg.idx);
}

void UEULWorker::run_thread()
{
  while (running.load()) {
    std::unique_lock<std::mutex> lock(mutex);
    if (current_task == nullptr) {
      std::this_thread::sleep_for(std::chrono::microseconds(100));
      continue;
    }
    cv.wait(lock, [this] { return grant_available || !running.load(); });
    if (!running.load()) {
      break;
    }
    if (!grant_available) {
      continue;
    }
    send_pusch(target_slot, current_task->msg, current_task->rx_slot_idx, current_task->rx_timestamp, target_dci);
  }
}