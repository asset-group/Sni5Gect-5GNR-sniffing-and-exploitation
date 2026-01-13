#include "shadower/comp/workers/broadcast_worker.h"
#include "shadower/utils/phy_cfg_utils.h"
#include "shadower/utils/ue_dl_utils.h"

BroadCastWorker::BroadCastWorker(ShadowerConfig& config_) :
  logger(srslog::fetch_basic_logger("BCWorker")), config(config_)
{
  slot_per_sf = 1 << config.scs_common;
  sf_len      = config.sample_rate * SF_DURATION;
  slot_len    = sf_len / slot_per_sf;
  logger.set_level(config.bc_worker_level);
  /* Initialize phy cfg */
  init_phy_cfg(phy_cfg, config);

  /* Initialize the input buffer */
  rx_buffer = srsran_vec_cf_malloc(sf_len);
  if (!rx_buffer) {
    throw std::runtime_error("Error allocating rx buffer");
  }
  if (!init_ue_dl(ue_dl, rx_buffer, phy_cfg)) {
    throw std::runtime_error("Error initializing UE DL NR");
  }

  /* physical state to help track downlink grants */
  phy_state.stack                    = nullptr;
  phy_state.args.nof_carriers        = 1;
  phy_state.args.dl.nof_rx_antennas  = config.nof_channels;
  phy_state.args.dl.nof_max_prb      = config.nof_prb;
  phy_state.args.dl.pdsch.max_prb    = config.nof_prb;
  phy_state.args.ul.nof_max_prb      = config.nof_prb;
  phy_state.args.ul.pusch.max_prb    = config.nof_prb;
  phy_state.args.ul.pusch.max_layers = config.nof_channels;

  /* Pre-initialize softbuffer rx */
  if (srsran_softbuffer_rx_init_guru(&softbuffer_rx, SRSRAN_SCH_NR_MAX_NOF_CB_LDPC, SRSRAN_LDPC_MAX_LEN_ENCODED_CB) !=
      0) {
    logger.error("Couldn't allocate and/or initialize softbuffer");
    throw std::runtime_error("Error initializing softbuffer");
  }

#if ENABLE_CUDA
  if (config.enable_gpu) {
    fft_processor =
        new FFTProcessor(config.sample_rate, ue_dl.carrier.dl_center_frequency_hz, phy_cfg.carrier.scs, &ue_dl.fft[0]);
  }
#endif // ENABLE_CUDA
}

bool BroadCastWorker::work(const std::shared_ptr<Task>& task)
{
  if (rnti == SRSRAN_INVALID_RNTI) {
    logger.error("RNTI not set");
    return false;
  }
  for (uint32_t slot_in_sf = 0; slot_in_sf < slot_per_sf; slot_in_sf++) {
    slot_cfg.idx = task->slot_idx + slot_in_sf;
    /* only copy half of the subframe to the buffer */
    srsran_vec_cf_copy(rx_buffer, task->dl_buffer[0]->data() + slot_in_sf * slot_len, slot_len);
/* estimate FFT will run on first slot */
#if ENABLE_CUDA
    if (config.enable_gpu) {
      fft_processor->to_ofdm(rx_buffer, ue_dl.sf_symbols[0], slot_cfg.idx);
    } else {
      srsran_ue_dl_nr_estimate_fft(&ue_dl, &slot_cfg);
    }
#else
    srsran_ue_dl_nr_estimate_fft(&ue_dl, &slot_cfg);
#endif // ENABLE_CUDA
    std::array<srsran_dci_dl_nr_t, SRSRAN_SEARCH_SPACE_MAX_NOF_CANDIDATES_NR> dci_dl = {};
    std::array<srsran_dci_ul_nr_t, SRSRAN_SEARCH_SPACE_MAX_NOF_CANDIDATES_NR> dci_ul = {};
    /* Estimate PDCCH channel and search for both dci ul and dci dl */
    ue_dl_dci_search(ue_dl, phy_cfg, slot_cfg, rnti, rnti_type, phy_state, logger, task->task_idx, dci_dl, dci_ul);
    /* PDSCH decoding */
    pdsch_decode(slot_cfg.idx, task->task_idx);
  }
  return true;
}

bool BroadCastWorker::pdsch_decode(uint32_t slot_idx, uint32_t task_idx)
{
  /* Get available grants from dci search */
  if (!phy_state.get_dl_pending_grant(slot_cfg.idx, pdsch_cfg, ack_resource, pid)) {
    return false;
  }
  /* Initialize the buffer for output*/
  srsran::unique_byte_buffer_t data = srsran::make_byte_buffer();
  if (data == nullptr) {
    logger.error("Error creating byte buffer");
    return false;
  }
  data->N_bytes = pdsch_cfg.grant.tb[0].tbs / 8U;
  /* Initialize pdsch result*/
  srsran_pdsch_res_nr_t pdsch_res = {};
  pdsch_res.tb[0].payload         = data->msg;
  /* Decode PDSCH */
  if (!ue_dl_pdsch_decode(ue_dl, pdsch_cfg, slot_cfg, pdsch_res, softbuffer_rx, logger, task_idx)) {
    return false;
  }
  /* if the message is not decoded correctly, then return */
  if (!pdsch_res.tb[0].crc) {
    logger.debug("Error PDSCH got wrong CRC");
    return false;
  }
  bool all_zeros = true;
  for (uint32_t a = 0; a < data->N_bytes; a++) {
    if (data->msg[a] != 0) {
      all_zeros = false;
      break;
    }
  }
  if (all_zeros) {
    logger.info("Broadcast worker received a message with all zeros");
    return false;
  }
  if (pdsch_cfg.grant.rnti_type == srsran_rnti_type_ra) {
    return decode_rar(data, slot_idx, task_idx);
  } else {
    if (decode_sib1(data)) {
      logger.info("SIB1 received successfully slot idx: %u task idx: %u", slot_idx, task_idx);
      on_sib1_found(sib1);
    }
    return true;
  }
}

/* If SIB1 is received, use the following function to decode SIB1 message and call the handler to update worker configs
 */
bool BroadCastWorker::decode_sib1(srsran::unique_byte_buffer_t& data)
{
  asn1::rrc_nr::sib1_s sib1_;
  if (!parse_to_sib1(data->msg, data->N_bytes, sib1_)) {
    logger.error("Error decoding SIB1");
    return false;
  }
  if (logger.debug.enabled()) {
    asn1::json_writer json_writer;
    sib1_.to_json(json_writer);
    logger.debug("SIB1: %s", json_writer.to_string().c_str());
  }
  sib1 = std::move(sib1_);
  return true;
}

/* If this is received from broadcast worker, then unpack using mac rar pdu and call the handler */
bool BroadCastWorker::decode_rar(srsran::unique_byte_buffer_t& data, uint32_t slot_idx, uint32_t task_idx)
{
  srsran::mac_rar_pdu_nr rar_pdu;
  if (!rar_pdu.unpack(data->msg, data->N_bytes)) {
    logger.error("Error decoding RACH msg2");
    return false;
  }
  uint32_t num_subpdus = rar_pdu.get_num_subpdus();
  if (num_subpdus == 0) {
    logger.error("No subpdus in RAR");
    return false;
  }
  const srsran::mac_rar_subpdu_nr& subpdu  = rar_pdu.get_subpdu(0);
  uint16_t                         tc_rnti = subpdu.get_temp_crnti();
  if (tc_rnti == SRSRAN_INVALID_RNTI) {
    logger.error("Invalid TC-RNTI");
    return false;
  }
  uint32_t time_advance = subpdu.get_ta();

  logger.info(CYAN "Found new UE with tc-rnti: %d slot: %u task: %u" RESET, tc_rnti, slot_idx, task_idx);
  std::array<uint8_t, srsran::mac_rar_subpdu_nr::UL_GRANT_NBITS> rar_grant = subpdu.get_ul_grant();
  on_ue_found(tc_rnti, rar_grant, slot_idx, time_advance);
  return true;
}

bool BroadCastWorker::apply_config_from_mib(srsran_mib_nr_t& mib_, uint32_t ncellid_)
{
  mib     = mib_;
  ncellid = ncellid_;
  if (!update_phy_cfg_from_mib(phy_cfg, mib, ncellid)) {
    return false;
  }
  update_ue_dl(ue_dl, phy_cfg);
#if ENABLE_CUDA
  if (config.enable_gpu) {
    fft_processor->set_phase_compensation(phy_cfg.carrier.dl_center_frequency_hz);
  }
#endif // ENABLE_CUDA
  logger.info("MIB applied to broadcast worker");
  return true;
}

bool BroadCastWorker::apply_config_from_sib1(asn1::rrc_nr::sib1_s& sib1_)
{
  sib1 = std::move(sib1_);
  update_phy_cfg_from_sib1(phy_cfg, sib1);
  update_ue_dl(ue_dl, phy_cfg);
#if ENABLE_CUDA
  if (config.enable_gpu) {
    fft_processor->set_phase_compensation(phy_cfg.carrier.dl_center_frequency_hz);
  }
#endif // ENABLE_CUDA
  logger.info("SIB1 applied to broadcast worker");
  return true;
}