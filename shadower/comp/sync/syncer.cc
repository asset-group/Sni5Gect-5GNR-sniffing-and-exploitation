#include "shadower/comp/sync/syncer.h"
#include "shadower/utils/utils.h"
#include "srsran/phy/utils/vector.h"

Syncer::Syncer(syncer_args_t args_, Source* source_, ShadowerConfig& config_) :
  source(source_),
  srate(args_.srate),
  config(config_),
  sf_len(srate * SF_DURATION),
  srsran::thread("Syncer"),
  slot_per_sf(1 << (uint32_t)args_.scs),
  buffer_pool(std::make_unique<SharedBufferPool>(sf_len, config_.pool_size))
{
  args = args_;
  logger.set_level(config.syncer_log_level);
  num_channels = config.nof_channels;
}

bool Syncer::init()
{
  /* initialize ssb object and internal buffer */
  srsran_ssb_args_t ssb_args = {};
  ssb_args.max_srate_hz      = args.srate;
  ssb_args.min_scs           = args.scs;
  ssb_args.enable_search     = true;
  ssb_args.enable_measure    = true;
  ssb_args.enable_decode     = true;
  if (srsran_ssb_init(&ssb, &ssb_args) < SRSRAN_SUCCESS) {
    logger.debug("Error srsran_ssb_init");
    return false;
  }
  /* set the ssb configs from the args */
  srsran_ssb_cfg_t ssb_cfg = {};
  ssb_cfg.srate_hz         = args.srate;
  ssb_cfg.center_freq_hz   = args.dl_freq;
  ssb_cfg.ssb_freq_hz      = args.ssb_freq;
  ssb_cfg.scs              = args.scs;
  ssb_cfg.pattern          = args.pattern;
  ssb_cfg.duplex_mode      = args.duplex_mode;
  ssb_cfg.periodicity_ms   = 10;
  if (srsran_ssb_set_cfg(&ssb, &ssb_cfg) < SRSRAN_SUCCESS) {
    logger.debug("Error srsran_ssb_set_cfg");
    return false;
  }

  tracer_sib1.init("ipc:///tmp/sni5gect.dl-sib1");
  tracer_sib1.set_throttle_ms(500);

  tracer_status.init("ipc:///tmp/sni5gect");
  tracer_status.set_throttle_ms(200);
#if ENABLE_CUDA
  if (config.enable_gpu) {
    ssb_cuda = new SSBCuda(srate, args.dl_freq, args.ssb_freq, args.scs, args.pattern, args.duplex_mode);
  }
#endif // ENABLE_CUDA
  running.store(true);
  return true;
}

void Syncer::stop()
{
  running.store(false);
  logger.info("Waiting syncer to exit");
  thread_cancel();
#if ENABLE_CUDA
  ssb_cuda->cleanup();
#endif // ENABLE_CUDA
}

void Syncer::run_tti()
{
  /* update the slot index if it is greater than 1 */
  srsran_timestamp_t temp = {};
  srsran_timestamp_copy(&temp, &timestamp_new);
  srsran_timestamp_sub(&temp, timestamp_prev.full_secs, timestamp_prev.frac_secs);
  int32_t tti_jump = static_cast<int32_t>(srsran_timestamp_uint64(&temp, 1e3)) * slot_per_sf;
  if (tti_jump != 0) {
    srsran_timestamp_copy(&timestamp_prev, &timestamp_new);
    tti = (tti + tti_jump) % (10240 * slot_per_sf);
  }
}

void Syncer::get_tti(uint32_t* idx, srsran_timestamp_t* ts)
{
  std::lock_guard<std::mutex> lock(time_mtx);
  *idx = tti;
  srsran_timestamp_copy(ts, &timestamp_new);
}

/* Correct:  |--s-----------|--------------|
Received:  |----s---------|--   Delay  2 symbols Copy --s--------- then receive |-- process the slot again
Received:    --|s-------------| Delay -2 symbols Copy --| and then receive ------------| process the new slot*/
bool Syncer::listen(std::shared_ptr<samples_t>& samples)
{
  cf_t* buffer[SRSRAN_MAX_CHANNELS];
  for (int i = 0; i < SRSRAN_MAX_CHANNELS; i++) {
    if (i < num_channels) {
      buffer[i] = samples->dl_buffer[i]->data();
    } else {
      buffer[i] = nullptr; // Fill the rest with nullptr if fewer channels
    }
  }

  /* receive data */
  uint32_t offset     = 0;
  uint32_t to_receive = sf_len;
  int32_t  limit      = 2.4e-6 * srate;
  if (samples_delayed > limit) {
    /* If there's still a lot of samples belong to last subframe not processed,
      we receive the remaining samples and make it complete */
    /* if current frame to receive still contain last frame, and the offset is larger than 500
    Then copy the last sf from the correct start to current buffer, we re-do some decoding on the
    same sf we already processed before */
    std::shared_ptr<samples_t> history = history_samples_queue.back();
    /* Remaining correctly aligned samples in the last slot */
    uint32_t remaining = sf_len - samples_delayed;
    /* read from history queue and fill current subframe with last subframe data */
    for (uint32_t i = 0; i < num_channels; i++) {
      srsran_vec_cf_copy(buffer[i], history->dl_buffer[i]->data() + samples_delayed, remaining);
    }
    offset     = remaining;
    to_receive = samples_delayed;
  } else if (samples_delayed > 0) {
    /* If the offset is too small, just ignore */
    srsran_timestamp_t ts;
    source->recv(buffer, samples_delayed, &ts);
  } else {
    /* if part of new frame is already occupied in last frame */
    offset     = (uint32_t)(-samples_delayed);
    to_receive = (sf_len + samples_delayed);
    if (offset > limit) {
      std::shared_ptr<samples_t> history = history_samples_queue.back();
      for (uint32_t i = 0; i < num_channels; i++) {
        srsran_vec_cf_copy(buffer[i], history->dl_buffer[i]->data() + to_receive, offset);
      }
    } else {
      for (uint32_t i = 0; i < num_channels; i++) {
        srsran_vec_cf_zero(buffer[i], offset);
      }
    }
  }
  /* reset next sample offset */
  samples_delayed = 0;
  srsran_timestamp_t ts;
  cf_t*              tmp[SRSRAN_MAX_CHANNELS];
  for (int i = 0; i < num_channels; i++) {
    tmp[i] = buffer[i] + offset;
  }
  /* receive the remaining samples of the subframe */
  if (source->recv(tmp, to_receive, &ts) == -1) {
    logger.debug("Error source->receive");
    return false;
  }

  /* maintain the history queue size to 11 */
  if (history_samples_queue.size() < 11) {
    history_samples_queue.push(samples);
  } else {
    history_samples_queue.pop();
    history_samples_queue.push(samples);
  }

  for (uint32_t i = 0; i < num_channels; i++) {
    srsran_vec_apply_cfo(buffer[i] + offset, -cfo_hz / srate, samples->dl_buffer[i]->data() + offset, to_receive);
  }

  std::lock_guard<std::mutex> lock(time_mtx);
  /* update the new received sample timer */
  srsran_timestamp_copy(&timestamp_new, &ts);
  /* update the slot index */
  run_tti();
  return true;
}

bool Syncer::run_cell_search()
{
  srsran_ssb_search_res_t cs_result = {};
  while (!cell_found.load()) {
    /* Initialize the buffer */
    std::shared_ptr<samples_t> samples = std::make_shared<samples_t>();
    for (int i = 0; i < config.nof_channels; i++) {
      samples->dl_buffer[i] = buffer_pool->get_buffer();
    }
    /* receive the samples */
    if (!listen(samples)) {
      logger.debug("Error receive samples for cell search");
      error_handler();
      return false;
    }

    /* run ssb search on new subframe received */
    if (srsran_ssb_search(&ssb, samples->dl_buffer[0]->data(), sf_len, &cs_result) < SRSRAN_SUCCESS) {
      logger.debug("Error srsran_ssb_search");
      continue;
    }
    /* if snr too low or crc error, skip the current subframe */
    if (cs_result.measurements.snr_dB < -10.0f || !cs_result.pbch_msg.crc) {
      samples_delayed = -0.01 * sf_len;
      logger.debug("SNR too small or crc error");
      continue;
    }
    /* extract mib from the pbch msg */
    if (!handle_pbch(cs_result.pbch_msg)) {
      logger.debug("Error handle_pbch");
      continue;
    }
    /* update the offset and the cfo */
    handle_measurements(cs_result.measurements);
    /* log out the cell information */
    std::array<char, 512> mib_info_str = {};
    srsran_pbch_msg_nr_mib_info(&mib, mib_info_str.data(), (uint32_t)mib_info_str.size());
    logger.info(YELLOW "Found cell: %s" RESET, mib_info_str.data());
    logger.debug("Cell Search CFO: %f", cs_result.measurements.cfo_hz);
    ncellid = cs_result.N_id;

    tracer_sib1.send(samples->dl_buffer[0]->data(), sf_len, true);
    cell_found.store(true);
    return true;
  }
  return false;
}

bool Syncer::handle_pbch(srsran_pbch_msg_nr_t& pbch_msg_)
{
  srsran_mib_nr_t tmp_mib = {};
  /* unpack the mib */
  if (srsran_pbch_msg_nr_mib_unpack(&pbch_msg_, &tmp_mib) != 0) {
    logger.debug("Error srsran_pbch_msg_nr_mib_unpack");
    return false;
  }
  if (tmp_mib.cell_barred) {
    logger.debug("Cell barred");
    return false;
  }
  /* update the class mib record */
  mib = tmp_mib;
  /* update the subframe index */
  uint32_t sf_idx = srsran_ssb_candidate_sf_idx(&ssb, pbch_msg_.ssb_idx, pbch_msg_.hrf);
  /* Update the TTI value */
  tti = (mib.sfn * 10 * slot_per_sf + sf_idx) % (10240 * slot_per_sf);
  return true;
}

/* update the sample offset and cfo for receiving the samples next time. */
void Syncer::handle_measurements(srsran_csi_trs_measurements_t& feedback)
{
  srsran_vec_zero((void*)&measurements, sizeof(srsran_csi_trs_measurements_t));
  srsran_combine_csi_trs_measurements(&measurements, &feedback, &measurements);
  samples_delayed = (uint32_t)round((double)feedback.delay_us * (srate * 1e-6));
  cfo_hz          = feedback.cfo_hz;
  measurements    = feedback;
  // logger.debug("CFO: %f SNR: %f", feedback.cfo_hz, feedback.snr_dB);
  tracer_status.send_string(fmt::format(
      "{{\"CFO\": {:.2f}, \"SNR\": {:.2f}, \"RSRP\": {:.2f}}}", feedback.cfo_hz, feedback.snr_dB, feedback.rsrp_dB));
}

/* If the syncer get out of sync, run this function to get back to sync again */
bool Syncer::run_sync_find(cf_t* buffer)
{
  srsran_csi_trs_measurements_t measurements_tmp = {};
  srsran_pbch_msg_nr_t          pbch_msg_tmp     = {};
#if ENABLE_CUDA
  if (config.enable_gpu) {
    if (ssb_cuda->ssb_run_sync_find(buffer, ncellid, &measurements_tmp, &pbch_msg_tmp) < 0) {
      logger.debug("Error ssb_run_sync_find");
      return false;
    }
  } else {
#endif // ENABLE_CUDA
    if (srsran_ssb_find(&ssb, buffer, ncellid, &measurements_tmp, &pbch_msg_tmp) < SRSRAN_SUCCESS) {
      logger.debug("Error srsran_ssb_find");
      return false;
    }
#if ENABLE_CUDA
  }
#endif /// ENABLE_CUDA
  if (!pbch_msg_tmp.crc) {
    logger.debug("run_sync_find PBCH CRC error %u", (uint32_t)tti);
    return false;
  }
  if (!handle_pbch(pbch_msg_tmp)) {
    logger.debug("Error handle_pbch");
    return false;
  }
  handle_measurements(measurements_tmp);
  in_sync.store(true);
  return true;
}

/* keep track of syncing status and detect if lost sync */
bool Syncer::run_sync_track(cf_t* buffer)
{
  if (tti.load() % config.ssb_period != 0) {
    return true;
  }
  srsran_csi_trs_measurements_t measurements_tmp = {};
  srsran_pbch_msg_nr_t          pbch_msg_tmp     = {};
  uint32_t                      half_frame       = (tti % SRSRAN_NOF_SF_X_FRAME) / (SRSRAN_NOF_SF_X_FRAME / 2);
  if (srsran_ssb_track(&ssb, buffer, ncellid, pbch_msg_tmp.ssb_idx, half_frame, &measurements_tmp, &pbch_msg_tmp) !=
      0) {
    logger.debug("Error srsran_ssb_track");
    return false;
  }
  if (!pbch_msg_tmp.crc) {
    logger.debug("run_sync_track PBCH CRC error %u", (uint32_t)tti);
    return false;
  }
  if (measurements_tmp.delay_us > 2.4f) {
    logger.debug("Delay too large");
    return false;
  }
  if (!handle_pbch(pbch_msg_tmp)) {
    logger.debug("Error handle_pbch");
    return false;
  }
  tracer_sib1.send(buffer, sf_len);
  handle_measurements(measurements_tmp);
  return true;
}

void Syncer::run_thread()
{
  /* Run cell search first to find the cell */
  if (!run_cell_search()) {
    logger.debug("Error run_cell_search");
    return;
  }

  if (!cell_found.load()) {
    logger.debug("Cell not found");
    running.store(false);
    return;
  }
  on_cell_found(mib, ncellid);
  std::shared_ptr<samples_t> last_sample = history_samples_queue.back();
#if ENABLE_CUDA
  if (config.enable_gpu) {
    ssb_cuda->init(SRSRAN_NID_2_NR(ncellid));
  }
#endif /// ENABLE_CUDA
  run_sync_find(last_sample->dl_buffer[0]->data());
  while (running.load()) {
    std::shared_ptr<samples_t> last_samples = history_samples_queue.back();
    std::shared_ptr<samples_t> samples      = std::make_shared<samples_t>();
    for (int i = 0; i < config.nof_channels; i++) {
      samples->dl_buffer[i] = buffer_pool->get_buffer();
    }
    if (!listen(samples)) {
      logger.error("Error receiving samples from source %u", task_idx);
      error_handler();
      return;
    }
    if (in_sync) {
      if (!run_sync_track(samples->dl_buffer[0]->data())) {
        in_sync.store(false);
        logger.debug("run_sync_track lost sync at %u", (uint32_t)tti);
        /* if lost sync try to run sync find to get back to sync */
        if (!run_sync_find(samples->dl_buffer[0]->data())) {
          logger.debug("Still out of sync run_sync_find: %u", (uint32_t)tti);
        } else {
          logger.debug("Get back to sync for slot: %u", (uint32_t)tti);
        }
      }
    } else {
      if (!run_sync_find(samples->dl_buffer[0]->data())) {
        logger.debug("Not in sync tti: %u", (uint32_t)tti);
      } else {
        logger.debug("Get to sync at slot: %u", (uint32_t)tti);
      }
    }
    for (uint32_t i = 0; i < num_channels; i++) {
      srsran_vec_apply_cfo(samples->dl_buffer[i]->data(), -cfo_hz / srate, samples->dl_buffer[i]->data(), sf_len);
    }
    std::shared_ptr<Task> task = std::make_shared<Task>();
    if (args.duplex_mode == SRSRAN_DUPLEX_MODE_FDD && source->nof_channels > 1 && source->nof_channels % 2 == 0) {
      for (uint32_t i = 0; i < num_channels; i += 2) {
        int index                   = i / 2;
        task->dl_buffer[index]      = samples->dl_buffer[i];
        task->ul_buffer[index]      = samples->dl_buffer[i + 1];
        task->last_dl_buffer[index] = last_samples->dl_buffer[i];
        task->last_ul_buffer[index] = last_samples->dl_buffer[i + 1];
      }
    } else {
      for (uint32_t i = 0; i < num_channels; i++) {
        task->dl_buffer[i]      = samples->dl_buffer[i];
        task->ul_buffer[i]      = samples->dl_buffer[i];
        task->last_dl_buffer[i] = last_samples->dl_buffer[i];
        task->last_ul_buffer[i] = last_samples->dl_buffer[i];
      }
    }

    task->slot_idx = tti;
    task->task_idx = task_idx++;
    task->ts       = timestamp_new;
    publish_subframe(task);
    if (config.enable_recorder) {
      char filename[64];
      for (int ch = 0; ch < config.nof_channels; ch++) {
        sprintf(filename, "sf_%u_%u_ch_%d", task->task_idx, task->slot_idx, ch);
        write_record_to_file(samples->dl_buffer[ch]->data(), sf_len, filename);
      }
    }
  }
}