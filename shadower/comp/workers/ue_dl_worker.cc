#include "shadower/comp/workers/ue_dl_worker.h"
#include "shadower/utils/constants.h"
#include "shadower/utils/utils.h"
#include "srsran/mac/mac_sch_pdu_nr.h"
#include <iomanip>
#include <sstream>
#include <utility>

// Tracer singleton
TraceSamples UEDLWorker::tracer_dl_pdsch;
TraceSamples UEDLWorker::tracer_dl_dci_ul;

UEDLWorker::UEDLWorker(srslog::basic_logger&             logger_,
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

UEDLWorker::~UEDLWorker()
{
  if (buffer) {
    free(buffer);
    buffer = nullptr;
  }
  srsran_ue_dl_nr_free(&ue_dl);
}

bool UEDLWorker::init(srsran::phy_cfg_nr_t& phy_cfg_)
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
  /* Init ue_dl instance */
  if (!init_ue_dl(ue_dl, buffer, phy_cfg)) {
    logger.error("Error initializing ue_dl");
    return false;
  }
  /* Initialize softbuffer rx */
  if (srsran_softbuffer_rx_init_guru(&softbuffer_rx, SRSRAN_SCH_NR_MAX_NOF_CB_LDPC, SRSRAN_LDPC_MAX_LEN_ENCODED_CB) !=
      0) {
    logger.error("Couldn't allocate and/or initialize softbuffer");
    return false;
  }

  UEDLWorker::tracer_dl_pdsch.init("ipc:///tmp/sni5gect.dl-pdsch");
  UEDLWorker::tracer_dl_pdsch.set_throttle_ms(100);

  UEDLWorker::tracer_dl_dci_ul.init("ipc:///tmp/sni5gect.dl-dci-ul");
  UEDLWorker::tracer_dl_dci_ul.set_throttle_ms(100);

#if ENABLE_CUDA
  if (config.enable_gpu) {
    fft_processor =
        new FFTProcessor(config.sample_rate, ue_dl.carrier.dl_center_frequency_hz, ue_dl.carrier.scs, &ue_dl.fft[0]);
  }
#endif // ENABLE_CUDA
  return true;
}

/* Update the UE DL configurations */
bool UEDLWorker::update_cfg(srsran::phy_cfg_nr_t& phy_cfg_)
{
  phy_cfg = phy_cfg_;
  if (!update_ue_dl(ue_dl, phy_cfg)) {
    logger.error("Failed to update ue_dl with new phy_cfg");
    return false;
  }
#if ENABLE_CUDA
  if (config.enable_gpu) {
    fft_processor->set_phase_compensation(phy_cfg.carrier.dl_center_frequency_hz);
  }
#endif // ENABLE_CUDA
  return true;
}

/* Update the pending RRC setup */
void UEDLWorker::update_pending_rrc_setup(bool pending_rrc_setup_)
{
  pending_rrc_setup = pending_rrc_setup_;
}

/* Update the rnti */
void UEDLWorker::set_rnti(uint16_t rnti_, srsran_rnti_type_t rnti_type_)
{
  std::lock_guard<std::mutex> lock(mutex);
  rnti      = rnti_;
  rnti_type = rnti_type_;
  UEDLWorker::tracer_dl_dci_ul.reset_throttle();
  UEDLWorker::tracer_dl_pdsch.reset_throttle();
}

/* Set the task for the ue_dl worker */
void UEDLWorker::set_task(std::shared_ptr<Task> task_)
{
  std::lock_guard<std::mutex> lock(mutex);
  task = std::move(task_);
}

void UEDLWorker::work_imp()
{
  if (!task) {
    return;
  }
  process_task(task);
}

/* Worker implementation, decode message send from base station to UE */
void UEDLWorker::process_task(std::shared_ptr<Task> task_)
{
  std::lock_guard<std::mutex> lock(mutex);
  task = std::move(task_);
  if (rnti == SRSRAN_INVALID_RNTI) {
    logger.error("RNTI not set");
    return;
  }
  /* Initialize the slot configuration */
  srsran_slot_cfg_t slot_cfg = {.idx = task->slot_idx};
  for (uint32_t slot_in_sf = 0; slot_in_sf < slot_per_sf; slot_in_sf++) {
    /* only copy half of the subframe to the buffer */
    slot_cfg.idx = task->slot_idx + slot_in_sf;
    /* only copy half of the subframe to the buffer */
    srsran_vec_cf_copy(buffer, task->dl_buffer[0]->data() + slot_in_sf * slot_len, slot_len);
    /* estimate FFT will run on first slot */
#if ENABLE_CUDA
    if (config.enable_gpu) {
      fft_processor->to_ofdm(buffer, ue_dl.sf_symbols[0], slot_cfg.idx);
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
    if (ue_dl.num_ul_dci > 0) {
      for (uint32_t i = 0; i < ue_dl.num_ul_dci; i++) {
        on_dci_ul_found(dci_ul[i], slot_cfg);
      }
    }
    /* PDSCH decoding */
    handle_pdsch(slot_cfg);
  }

  if (ue_dl.num_dl_dci > 0) {
    UEDLWorker::tracer_dl_pdsch.send(task->dl_buffer[0]->data(), sf_len);
  }
  if (ue_dl.num_ul_dci > 0) {
    UEDLWorker::tracer_dl_dci_ul.send(task->dl_buffer[0]->data(), sf_len);
  }
}

/* Decode the PDSCH message */
void UEDLWorker::handle_pdsch(srsran_slot_cfg_t& slot_cfg)
{
  /* Get available grants from dci search */
  if (!phy_state.get_dl_pending_grant(slot_cfg.idx, pdsch_cfg, ack_resource, pid)) {
    return;
  }
  /* Update the last received message time */
  update_rx_timestamp();
  /* Initialize the buffer for output*/
  srsran::unique_byte_buffer_t data = srsran::make_byte_buffer();
  if (data == nullptr) {
    logger.error("Error creating byte buffer");
    return;
  }
  /* Initialize pdsch result*/
  srsran_pdsch_res_nr_t pdsch_res = {};
  data->N_bytes                   = pdsch_cfg.grant.tb[0].tbs / 8U;
  pdsch_res.tb[0].payload         = data->msg;
  /* Decode PDSCH */
  if (!ue_dl_pdsch_decode(ue_dl, pdsch_cfg, slot_cfg, pdsch_res, softbuffer_rx, logger, task->task_idx)) {
    return;
  }
  /* If the message is not decoded correctly, then return */
  if (!pdsch_res.tb[0].crc) {
    logger.debug("Error PDSCH got wrong CRC");
    return;
  }

  /* Check if the message is all zeros */
  bool all_zeros = true;
  for (uint32_t i = 0; i < data->N_bytes; i++) {
    if (data->msg[i] != 0) {
      all_zeros = false;
      break;
    }
  }
  if (all_zeros) {
    return;
  }

  /* Write to pcap */
  pcap_writer->write_dl_crnti_nr(data->msg, data->N_bytes, task->task_idx, 0, slot_cfg.idx);
  /* Pass the decoded to wdissector */
  wd_worker->process(data->msg,
                     data->N_bytes,
                     rnti,
                     slot_cfg.idx / slot_per_frame,
                     slot_cfg.idx % slot_per_frame,
                     slot_cfg.idx,
                     DL,
                     exploit);
  /* If rrc_setup is found, then return */
  if (!config.parse_messages) {
    return;
  }
  /* Prevent the MAC PDU is too short when decoding */
  if (data->N_bytes < 4) {
    return;
  }

  /* Unpack the mac pdu */
  srsran::mac_sch_pdu_nr pdu;
  if (pdu.unpack(data->msg, data->N_bytes) != SRSRAN_SUCCESS) {
    logger.error("Failed to unpack MAC SDU");
    return;
  }
  /* Process each mac subpdu individually */
  uint32_t num_pdu = pdu.get_num_subpdus();
  for (uint32_t i = 0; i < num_pdu; i++) {
    srsran::mac_sch_subpdu_nr& subpdu = pdu.get_subpdu(i);
    switch (subpdu.get_lcid()) {
      case srsran::mac_sch_subpdu_nr::nr_lcid_sch_t::CCCH: {
        handle_dl_ccch(subpdu.get_sdu(), subpdu.get_sdu_length());
        break;
      }
      case 0b00000001: {
        handle_dlsch(subpdu.get_sdu(), subpdu.get_sdu_length());
        break;
      }
      case srsran::mac_sch_subpdu_nr::TA_CMD: {
        srsran::mac_sch_subpdu_nr::ta_t ta = subpdu.get_ta();
        logger.info("Timing Advance Command: %d", ta.ta_command);
        update_timing_advance(ta.ta_command);
        break;
      }
      case srsran::mac_sch_subpdu_nr::nr_lcid_sch_t::CON_RES_ID: {
        srsran::mac_sch_subpdu_nr::ue_con_res_id_t con_res_id = subpdu.get_ue_con_res_id_ce();
        std::ostringstream                         oss;
        for (uint32_t i = 0; i < srsran::mac_sch_subpdu_nr::ue_con_res_id_len; i++) {
          oss << std::setfill('0') << std::setw(2) << std::hex << static_cast<int>(con_res_id.data()[i]);
        }
        logger.info("Contention resolution ID: %s", oss.str().c_str());
        break;
      }
      case srsran::mac_sch_subpdu_nr::nr_lcid_sch_t::PADDING:
        break;
      default:
        break;
    }
  }
}

/* */
void UEDLWorker::handle_dlsch(uint8_t* sdu, uint32_t len)
{
  uint8_t* rrc_data = sdu;
  uint32_t rrc_len  = len;
  /* Decode the raw bytes into DL-SCH */
  if (*sdu & 0x80) {
    if (*sdu & 0x30) {
      logger.info("Skipping segmented DL-SCH");
      return;
    }
    /* AM data */
    rrc_data += 2; /* AM header */
    rrc_data += 2; /* PDCP header */
    rrc_len -= 4;
  } else {
    /* ACK SN */
    return;
  }

  /* Decode message into RRC DL_DCCH_msg*/
  asn1::rrc_nr::dl_dcch_msg_s dl_dcch_msg;
  asn1::cbit_ref              bref(rrc_data, rrc_len);
  asn1::SRSASN_CODE           err = dl_dcch_msg.unpack(bref);
  if (err != asn1::SRSASN_SUCCESS) {
    logger.error("Error unpacking DL-DCCH message");
    return;
  }

  if (dl_dcch_msg.msg.type().value != asn1::rrc_nr::dl_dcch_msg_type_c::types_opts::c1) {
    logger.error("Expected RRC message");
    return;
  }

  switch (dl_dcch_msg.msg.c1().type().value) {
    case asn1::rrc_nr::dl_dcch_msg_type_c::c1_c_::types::rrc_recfg: {
      asn1::rrc_nr::cell_group_cfg_s cell_group_cfg;
      asn1::rrc_nr::rrc_recfg_s&     rrc_recfg = dl_dcch_msg.msg.c1().rrc_recfg();
      asn1::cbit_ref                 bref_cg(rrc_recfg.crit_exts.rrc_recfg().non_crit_ext.master_cell_group.data(),
                             rrc_recfg.crit_exts.rrc_recfg().non_crit_ext.master_cell_group.size());
      if (cell_group_cfg.unpack(bref_cg) != asn1::SRSASN_SUCCESS) {
        logger.error("Could not unpack master cell group config");
        return;
      }
      apply_cell_group_cfg(cell_group_cfg);
      break;
    }

    case asn1::rrc_nr::dl_dcch_msg_type_c::c1_c_::types::rrc_release: {
      logger.info("RRC Release received");
      deactivate();
      break;
    }

    default:
      break;
  }
}

/* Handle CCCH message */
void UEDLWorker::handle_dl_ccch(uint8_t* sdu, uint32_t len)
{
  /* Decode the raw bytes into DL_CCCH*/
  asn1::rrc_nr::dl_ccch_msg_s dl_ccch_msg;
  if (!parse_to_dl_ccch_msg(sdu, len, dl_ccch_msg)) {
    logger.error("Failed to parse DL-CCCH message");
    return;
  }
  if (logger.debug.enabled()) {
    asn1::json_writer json_writer;
    dl_ccch_msg.msg.to_json(json_writer);
    logger.debug("CCCH message: %s", json_writer.to_string().c_str());
  }
  if (dl_ccch_msg.msg.c1().type().value == asn1::rrc_nr::dl_ccch_msg_type_c::c1_c_::types::rrc_reject) {
    logger.info(RED "RRC-Reject received" RESET);
    deactivate();
  } else if (dl_ccch_msg.msg.c1().type().value == asn1::rrc_nr::dl_ccch_msg_type_c::c1_c_::types::rrc_setup) {
    /* Decode the cell group data from rrc_setup message */
    asn1::rrc_nr::cell_group_cfg_s cell_group;
    if (!extract_cell_group_cfg(dl_ccch_msg, cell_group)) {
      logger.error("Failed to extract cell group config");
      return;
    }
    /* Update the phy_cfg with the cell group data */
    apply_cell_group_cfg(cell_group);
  }
}