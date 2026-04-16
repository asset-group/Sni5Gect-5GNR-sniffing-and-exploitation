#include <utility>

#include "shadower/comp/workers/gnb_ul_worker.h"
#include "shadower/utils/utils.h"

// Tracer singleton
TraceSamples GNBULWorker::tracer_ul_pusch;

GNBULWorker::GNBULWorker(srslog::basic_logger&             logger_,
                         ShadowerConfig&                   config_,
                         srsue::nr::state&                 phy_state_,
                         WDWorker*                         wd_worker_,
                         Exploit*                          exploit_,
                         std::shared_ptr<srsran::mac_pcap> pcap_writer_) :
  logger(logger_),
  config(config_),
  phy_state(phy_state_),
  pcap_writer(std::move(pcap_writer_)),
  wd_worker(wd_worker_),
  exploit(exploit_)
{
}

GNBULWorker::~GNBULWorker()
{
  if (buffer) {
    free(buffer);
    buffer = nullptr;
  }
  srsran_gnb_ul_free(&gnb_ul);
}

bool GNBULWorker::init(srsran::phy_cfg_nr_t& phy_cfg_)
{
  std::lock_guard<std::mutex> lock(mutex);
  phy_cfg        = phy_cfg_;
  srate          = config.sample_rate;
  sf_len         = srate * SF_DURATION;
  slot_per_sf    = 1 << config.scs_common;
  slot_per_frame = slot_per_sf * NUM_SUBFRAME;
  slot_len       = sf_len / slot_per_sf;
  nof_sc         = config.nof_prb * SRSRAN_NRE;
  nof_re         = nof_sc * SRSRAN_NSYMB_PER_SLOT_NR;
  numerology     = (uint32_t)config.scs_common;
  /* Init buffer */
  buffer = srsran_vec_cf_malloc(sf_len);
  if (!buffer) {
    logger.error("Error allocating buffer");
    return false;
  }
  /* Init gnb_ul instance */
  if (!init_gnb_ul(gnb_ul, buffer, phy_cfg)) {
    logger.error("Error initializing gnb_ul");
    return false;
  }
  /* Initialize softbuffer rx */
  if (srsran_softbuffer_rx_init_guru(&softbuffer_rx, SRSRAN_SCH_NR_MAX_NOF_CB_LDPC, SRSRAN_LDPC_MAX_LEN_ENCODED_CB) !=
      0) {
    logger.error("Couldn't allocate and/or initialize softbuffer");
    return false;
  }

  GNBULWorker::tracer_ul_pusch.init("ipc:///tmp/sni5gect.ul-pusch");
  GNBULWorker::tracer_ul_pusch.set_throttle_ms(250);

#if ENABLE_CUDA
  if (config.enable_gpu) {
    fft_processor =
        new FFTProcessor(config.sample_rate, gnb_ul.carrier.ul_center_frequency_hz, gnb_ul.carrier.scs, &gnb_ul.fft);
  }
#endif // ENABLE_CUDA
  return true;
}

/* Update the GNB UL configurations */
bool GNBULWorker::update_cfg(srsran::phy_cfg_nr_t& phy_cfg_)
{
  phy_cfg = phy_cfg_;
  if (!update_gnb_ul(gnb_ul, phy_cfg)) {
    logger.error("Failed to update gnb_ul with new phy_cfg");
    return false;
  }
#if ENABLE_CUDA
  if (config.enable_gpu) {
    fft_processor->set_phase_compensation(phy_cfg.carrier.ul_center_frequency_hz);
  }
#endif // ENABLE_CUDA
  return true;
}

/* Update the rnti */
void GNBULWorker::set_rnti(uint16_t rnti_, srsran_rnti_type_t rnti_type_)
{
  std::lock_guard<std::mutex> lock(mutex);
  rnti      = rnti_;
  rnti_type = rnti_type_;
  GNBULWorker::tracer_ul_pusch.reset_throttle();
}

/* Set the context for the gnb_ul worker */
void GNBULWorker::set_task(std::shared_ptr<Task> task_)
{
  std::lock_guard<std::mutex> lock(mutex);
  task = std::move(task_);
}

void GNBULWorker::work_imp()
{
  if (!task) {
    return;
  }
  process_task(task);
}

/* Worker implementation, decode message send from UE to base station */
void GNBULWorker::process_task(std::shared_ptr<Task> task_)
{
  std::lock_guard<std::mutex> lock(mutex);
  task = std::move(task_);
  if (rnti == SRSRAN_INVALID_RNTI) {
    logger.error("RNTI not set");
    return;
  }
  srsran_slot_cfg_t slot_cfg = {.idx = task->slot_idx};
  for (uint32_t slot_in_sf = 0; slot_in_sf < slot_per_sf; slot_in_sf++) {
    /* only copy half of the subframe to the buffer */
    slot_cfg.idx = task->slot_idx + slot_in_sf;
    if (!phy_state.get_ul_pending_grant(slot_cfg.idx, pusch_cfg, pid)) {
      continue;
    }
    /* Update the last received message time */
    update_rx_timestamp();
    /* Copy the samples to the process buffer */
    if (slot_in_sf == 0 && ta_samples > 0) {
      /* If it is the first slot, then part of the samples is in the last slot */
      srsran_vec_cf_copy(buffer, task->last_ul_buffer[0]->data() + sf_len - ta_samples, ta_samples);
      srsran_vec_cf_copy(buffer + ta_samples, task->ul_buffer[0]->data(), slot_len - ta_samples);
    } else {
      /* only copy half of the subframe to the buffer */
      srsran_vec_cf_copy(buffer, task->ul_buffer[0]->data() + slot_in_sf * slot_len - ta_samples, slot_len);
    }
/* estimate FFT will run on first slot */
#if ENABLE_CUDA
    if (config.enable_gpu) {
      fft_processor->to_ofdm(buffer, gnb_ul.sf_symbols[0], slot_cfg.idx);
    } else {
      srsran_gnb_ul_fft(&gnb_ul);
    }
#else
    srsran_gnb_ul_fft(&gnb_ul);
#endif // ENABLE_CUDA

    /* PUSCH search and decoding */
    handle_pusch(slot_cfg);

    /* Trace slot */
    GNBULWorker::tracer_ul_pusch.send(task->ul_buffer[0]->data(), sf_len);
  }
}

/* Handle PUSCH decoding */
void GNBULWorker::handle_pusch(srsran_slot_cfg_t& slot_cfg)
{
  /* Apply the CFO to correct the OFDM symbols */
  srsran_vec_apply_cfo(gnb_ul.sf_symbols[0], config.uplink_cfo, gnb_ul.sf_symbols[0], nof_re);
  /* Initialize the buffer for output */
  srsran::unique_byte_buffer_t data = srsran::make_byte_buffer();
  if (data == nullptr) {
    logger.error("Error creating byte buffer");
    return;
  }
  /* Initialize pusch result */
  srsran_pusch_res_nr_t pusch_res = {};
  data->N_bytes                   = pusch_cfg.grant.tb[0].tbs / 8U;
  pusch_res.tb[0].payload         = data->msg;
  /* Decode PUSCH */
  if (!gnb_ul_pusch_decode(gnb_ul, pusch_cfg, slot_cfg, pusch_res, softbuffer_rx, logger, task->task_idx)) {
    return;
  }
  /* If the message is not decoded correctly, then return */
  if (!pusch_res.tb[0].crc) {
    logger.debug("Error PUSCH got wrong CRC");
    return;
  }
  double   delay_offset   = pusch_res.delay_us * config.sample_rate;
  uint32_t old_ta_samples = ta_samples;
  if (std::abs(delay_offset) > 1 && ta_samples - delay_offset > 0) {
    uint32_t new_ta_samples = ta_samples - delay_offset;
    logger.info("Update the uplink message offset based on DMRS feedback %u => %u", ta_samples, new_ta_samples);
    update_offset_from_dmrs_feedback(new_ta_samples);
  }

  /* Write to pcap */
  pcap_writer->write_ul_crnti_nr(data->msg, data->N_bytes, task->task_idx, 0, slot_cfg.idx);
  /* Pass the decoded to wdissector */
  wd_worker->process(data->msg,
                     data->N_bytes,
                     rnti,
                     slot_cfg.idx / slot_per_frame,
                     slot_cfg.idx % slot_per_frame,
                     slot_cfg.idx,
                     UL,
                     exploit);
  if (config.expose_data) {
    uplink_api_hdr_t api_hdr         = {};
    api_hdr.message_type             = 1; // Uplink API message
    api_hdr.rnti                     = rnti;
    api_hdr.rnti_type                = (uint16_t)rnti_type;
    api_hdr.slot_idx                 = slot_cfg.idx;
    api_hdr.task_idx                 = task->task_idx;
    api_hdr.sf_len                   = sf_len;
    api_hdr.offset                   = ta_samples;
    srsran_timestamp_t toa_estimated = {};
    srsran_timestamp_copy(&toa_estimated, &task->ts);
    srsran_timestamp_sub(&toa_estimated, 0, SF_DURATION + ta_samples / srate);
    api_hdr.full_secs    = toa_estimated.full_secs;
    api_hdr.frac_secs    = toa_estimated.frac_secs;
    api_hdr.time_diff    = (double)old_ta_samples / srate - pusch_res.delay_us;
    api_hdr.nof_prb      = gnb_ul.carrier.nof_prb;
    api_hdr.start_symbol = pusch_cfg.grant.S;
    api_hdr.nof_symbol   = pusch_cfg.grant.L;
    api_hdr.snr_dB       = pusch_res.snr_dB;
    for (uint32_t i = 0; i < api_hdr.nof_prb; i++) {
      api_hdr.prb_map[i] = pusch_cfg.grant.prb_idx[i];
    }
    if (ul_api != nullptr && ul_api->is_initialized()) {
      if (ul_api->send_uplink_api_message(api_hdr, task->ul_buffer[0]->data(), task->last_ul_buffer[0]->data()) < 0) {
        logger.error("Failed to send uplink API message for RNTI: %u", rnti);
      }
    }
  }
}