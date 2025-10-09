#include "shadower/comp/ue_tracker.h"
#include "shadower/comp/scheduler.h"
#include "shadower/utils/constants.h"
#include "srsran/asn1/rrc_nr_utils.h"

UETracker::UETracker(Source*           source_,
                     Syncer*           syncer_,
                     WDWorker*         wd_worker_,
                     ShadowerConfig&   config_,
                     create_exploit_t& create_exploit_handler) :
  source(source_),
  syncer(syncer_),
  wd_worker(wd_worker_),
  config(config_),
  exploit_creator(create_exploit_handler),
  srsran::thread("UETracker"),
  ue_dl_pool(config_.n_ue_dl_worker),
  gnb_ul_pool(config_.n_gnb_ul_worker),
  gnb_dl_pool(config_.n_gnb_dl_worker)
{
  logger.set_level(config.worker_log_level);

  /* initialize phy cfg and update with mib and sib1 */
  init_phy_cfg(phy_cfg, config);

  /* Initialize phy_state */
  init_phy_state(phy_state, config.nof_prb);
  init_phy_state(ul_phy_state, config.nof_prb);

  /* Create the exploit */
  exploit = exploit_creator(dl_msg_queue, ul_msg_queue);
  exploit->setup();

  /* Each RNTI have it's specific pcap writer */
  pcap_writer = std::make_unique<srsran::mac_pcap>();
}

UETracker::~UETracker()
{
  deactivate();
}

/* Activate the current UETracker */
void UETracker::activate(uint16_t rnti_, srsran_rnti_type_t rnti_type_, uint32_t time_advance)
{
  rnti             = rnti_;
  rnti_type        = rnti_type_;
  name             = "UE-" + std::to_string(rnti);
  n_timing_advance = time_advance * 16 * 64 / (1 << config.scs_common) + phy_cfg.t_offset;
  ta_time          = static_cast<double>(n_timing_advance) * Tc;

  /* Update the rnti for ue dl */
  for (uint32_t i = 0; i < config.n_ue_dl_worker; i++) {
    UEDLWorker* w = ue_dl_workers[i];
    w->set_rnti(rnti, rnti_type);
    w->update_timing_advance = std::bind(&UETracker::update_timing_advance, this, std::placeholders::_1);
  }
  /* Update the rnti for gnb ul */
  for (uint32_t i = 0; i < config.n_gnb_ul_worker; i++) {
    GNBULWorker* w = gnb_ul_workers[i];
    w->set_rnti(rnti, rnti_type);
    w->set_ta_samples(ta_time);
  }
  /* Initialize the pcap writer */
  if (pcap_writer->open(config.pcap_folder + name + ".pcap")) {
    logger.error("Failed to open pcap file");
  }
  active.store(true);

  /* Update last received message timestamp */
  last_message_time = std::chrono::steady_clock::now();
  /* Initialize the gnb_dl thread */
  start(3); // Between Syncer and Scheduler
  /* Start the UE UL worker thread */
  if (ue_ul_worker) {
    ue_ul_worker->begin();
    ue_ul_worker->start(0);
  }
  logger.info(GREEN "UETracker %s activated" RESET, name.c_str());
}

/* Deactivate the current UETracker */
void UETracker::deactivate()
{
  active.store(false);
  /* If the gnb dl thread is still active, then stop the thread */
  thread_cancel();
  ue_ul_worker->stop();
  ue_ul_worker->thread_cancel();
  pcap_writer->close();
  logger.info("Deactivated UETracker %s", name.c_str());
  logger.info("Capture saved to: %s", config.pcap_folder + name + ".pcap");
  on_deactivate();
}

void UETracker::update_timing_advance(int32_t ta_command)
{
  int32_t n_ta_old = n_timing_advance;
  n_timing_advance = n_ta_old + ((int32_t)ta_command - 31) * 16 * 64 / (1 << config.scs_common);
  ta_time          = static_cast<double>(n_timing_advance) * Tc;
  for (uint32_t i = 0; i < config.n_gnb_ul_worker; i++) {
    GNBULWorker* w = gnb_ul_workers[i];
    w->set_ta_samples(ta_time);
  }
}

int UETracker::on_dci_ul_found(srsran_dci_ul_nr_t& dci_ul, srsran_slot_cfg_t& slot_cfg)
{
  if (ue_ul_worker) {
    return ue_ul_worker->set_pusch_grant(dci_ul, slot_cfg);
  }
  return -1;
}

bool UETracker::init()
{
  /* Initialize the ue dl workers and add them to the worker pool */
  for (uint32_t i = 0; i < config.n_ue_dl_worker; i++) {
    UEDLWorker* w = new UEDLWorker(logger, config, phy_state, wd_worker, exploit, pcap_writer);
    ue_dl_workers.push_back(w);
    if (!w->init(phy_cfg)) {
      return false;
    }
    if (!w->update_cfg(phy_cfg)) {
      return false;
    }
    /* Bind the deactivate function to UETracker */
    w->deactivate = std::bind(&UETracker::deactivate, this);
    /* Bind the apply_cell_group_cfg function to UETracker */
    w->apply_cell_group_cfg = std::bind(&UETracker::apply_config_from_rrc_setup, this, std::placeholders::_1);
    /* Update the corresponding RX timestamp */
    w->update_rx_timestamp = std::bind(&UETracker::update_last_rx_timestamp, this);
    /* Bind the DCI UL found call back */
    w->on_dci_ul_found = std::bind(&UETracker::on_dci_ul_found, this, std::placeholders::_1, std::placeholders::_2);
    /* Initialize ue_dl worker in the work pool */
    ue_dl_pool.init_worker(i, w, 80);
  }

  /* Initialize the ue ul worker first and then bind the call back on the ue_ul worker */
  ue_ul_worker = new UEULWorker(logger, config, source, ul_phy_state);
  if (!ue_ul_worker->init(phy_cfg)) {
    return false;
  }
  if (!ue_ul_worker->update_cfg(phy_cfg)) {
    return false;
  }

  /* Initialize the gnb ul workers and add them to the worker pool */
  for (uint32_t i = 0; i < config.n_gnb_ul_worker; i++) {
    GNBULWorker* w = new GNBULWorker(logger, config, phy_state, wd_worker, exploit, pcap_writer);
    gnb_ul_workers.push_back(w);
    if (!w->init(phy_cfg)) {
      return false;
    }
    if (!w->update_cfg(phy_cfg)) {
      return false;
    }
    /* Update the corresponding RX timestamp */
    w->update_rx_timestamp = std::bind(&UETracker::update_last_rx_timestamp, this);
    /* Initialize gnb_ul worker in the work pool */
    gnb_ul_pool.init_worker(i, w, 80);
  }

  /* Initialize the gnb dl workers and add them to the worker pool */
  for (uint32_t i = 0; i < config.n_gnb_dl_worker; i++) {
    GNBDLWorker* w = new GNBDLWorker(logger, source, config);
    gnb_dl_workers.push_back(w);
    if (!w->init()) {
      return false;
    }
    if (!w->update_cfg(phy_cfg)) {
      return false;
    }
    gnb_dl_pool.init_worker(i, w, 80);
  }
  return true;
}

/* Apply the configuration from SIB1 */
bool UETracker::apply_config_from_sib1(asn1::rrc_nr::sib1_s& sib1)
{
  update_phy_cfg_from_sib1(phy_cfg, sib1);
  // By default set the t_offset to 25600
  phy_cfg.t_offset = 25600;
  if (sib1.serving_cell_cfg_common_present) {
    if (sib1.serving_cell_cfg_common.n_timing_advance_offset_present) {
      switch (sib1.serving_cell_cfg_common.n_timing_advance_offset.value) {
        case asn1::rrc_nr::serving_cell_cfg_common_sib_s::n_timing_advance_offset_opts::n0:
          phy_cfg.t_offset = 0;
          break;
        case asn1::rrc_nr::serving_cell_cfg_common_sib_s::n_timing_advance_offset_opts::n25600:
          phy_cfg.t_offset = 25600;
          break;
        case asn1::rrc_nr::serving_cell_cfg_common_sib_s::n_timing_advance_offset_opts::n39936:
          phy_cfg.t_offset = 39936;
          break;
        default:
          logger.error("Invalid n_ta_offset option");
          break;
      }
    }
  }
  if (!update_cfg()) {
    return false;
  }
  return true;
}

/* Apply the configuration from MIB */
bool UETracker::apply_config_from_mib(srsran_mib_nr_t& mib, uint32_t ncellid)
{
  if (!update_phy_cfg_from_mib(phy_cfg, mib, ncellid)) {
    return false;
  }
  if (!update_cfg()) {
    return false;
  }
  return true;
}

/* Apply the configuration from cell group */
bool UETracker::apply_config_from_rrc_setup(asn1::rrc_nr::cell_group_cfg_s& cell_group)
{
  /* Update the phy_cfg with the cell group data */
  if (cell_group.sp_cell_cfg_present) {
    if (!update_phy_cfg_from_cell_cfg(
            phy_cfg, cell_group.sp_cell_cfg, pucch_res_list, csi_rs_zp_res, csi_rs_nzp_res, logger)) {
      logger.info("Failed to update phy cfg from cell cfg");
      return false;
    }
  }
  /* Update the phy_cfg with the phys_cell_group_cfg data */
  if (cell_group.phys_cell_group_cfg_present) {
    switch (cell_group.phys_cell_group_cfg.pdsch_harq_ack_codebook) {
      case asn1::rrc_nr::phys_cell_group_cfg_s::pdsch_harq_ack_codebook_opts::dynamic_value:
        phy_cfg.harq_ack.harq_ack_codebook = srsran_pdsch_harq_ack_codebook_dynamic;
        break;
      case asn1::rrc_nr::phys_cell_group_cfg_s::pdsch_harq_ack_codebook_opts::semi_static:
        phy_cfg.harq_ack.harq_ack_codebook = srsran_pdsch_harq_ack_codebook_semi_static;
        break;
      case asn1::rrc_nr::phys_cell_group_cfg_s::pdsch_harq_ack_codebook_opts::nulltype:
        phy_cfg.harq_ack.harq_ack_codebook = srsran_pdsch_harq_ack_codebook_none;
        break;
      default:
        asn1::log_warning("Invalid option for pdsch_harq_ack_codebook %s",
                          cell_group.phys_cell_group_cfg.pdsch_harq_ack_codebook.to_string());
        return false;
    }
  }
  /* Update the cfg after rrc setup */
  if (!update_cfg()) {
    logger.error("Failed to update cfg after rrc_setup");
    return false;
  }
  /* For each ue_dl set rrc setup found */
  for (uint32_t i = 0; i < config.n_ue_dl_worker; i++) {
    UEDLWorker* w = ue_dl_workers[i];
    w->update_pending_rrc_setup(false);
  }
  return true;
}

/* Distribute the phy_cfg config */
bool UETracker::update_cfg()
{
  std::lock_guard<std::mutex> lock(cfg_mtx);
  bool                        ret = true;
  /* Apply the phy_cfg to all ue_dl */
  for (uint32_t i = 0; i < config.n_ue_dl_worker; i++) {
    UEDLWorker* w = ue_dl_workers[i];
    if (!w->update_cfg(phy_cfg)) {
      logger.error("Failed to update ue_dl with new phy_cfg");
      ret = false;
    }
  }
  /* Apply the phy_cfg to all gnb_dl */
  for (uint32_t i = 0; i < config.n_gnb_dl_worker; i++) {
    GNBDLWorker* w = gnb_dl_workers[i];
    if (!w->update_cfg(phy_cfg)) {
      logger.error("Failed to update gnb_dl with new phy_cfg");
      ret = false;
    }
  }
  /* Apply the phy_cfg to all gnb_ul */
  for (uint32_t i = 0; i < config.n_gnb_ul_worker; i++) {
    GNBULWorker* w = gnb_ul_workers[i];
    if (!w->update_cfg(phy_cfg)) {
      logger.error("Failed to update gnb_ul with new phy_cfg");
      ret = false;
    }
  }
  /* Apply the phy_cfg to ue_ul */
  if (ue_ul_worker) {
    if (!ue_ul_worker->update_cfg(phy_cfg)) {
      logger.error("Failed to update ue_ul with new phy_cfg");
      ret = false;
    }
  }
  return ret;
}

/* Set the rar grant for UE found in rach msg2 */
void UETracker::set_ue_rar_grant(std::array<uint8_t, srsran::mac_rar_subpdu_nr::UL_GRANT_NBITS>& grant,
                                 uint32_t                                                        slot_idx)
{
  if (!set_rar_grant(rnti, rnti_type, slot_idx, grant, phy_cfg, phy_state, logger)) {
    logger.error("Failed to set RAR grant for UE: %u", rnti);
  }
}

/* GNB DL thread implementation, keep retrieve task from the queue and run the task */
void UETracker::run_thread()
{
  /* Current active message */
  std::shared_ptr<std::vector<uint8_t> > current_msg = nullptr;
  /* Keep tracking how many time the message has been sent */
  uint32_t epoch_count = 0;
  /* Track last sent slot number, prevent sending the message in the same slot */
  uint32_t last_sent_slot = 0;
  while (active.load()) {
    /* Retrieve the current tracking rx slot index and timestamp*/
    uint32_t           rx_slot_idx;
    srsran_timestamp_t rx_timestamp;
    syncer->get_tti(&rx_slot_idx, &rx_timestamp);

    std::shared_ptr<std::vector<uint8_t> > ul_new_msg = ul_msg_queue.retrieve_non_blocking();
    if (ul_new_msg != nullptr) {
      /* If retrieved new message from the queue is not none, then set the context of ue_ul_worker */
      std::shared_ptr<UEULWorker::ue_ul_task_t> ul_task_ptr = std::make_shared<UEULWorker::ue_ul_task_t>();
      ul_task_ptr->rnti                                     = rnti;
      ul_task_ptr->rnti_type                                = rnti_type;
      ul_task_ptr->rx_slot_idx                              = rx_slot_idx;
      ul_task_ptr->rx_timestamp                             = rx_timestamp;
      ul_task_ptr->msg                                      = ul_new_msg;
      ue_ul_worker->set_context(ul_task_ptr);
    }

    std::shared_ptr<std::vector<uint8_t> > new_msg = dl_msg_queue.retrieve_non_blocking();
    /* If retrieved new message from the queue is not none, then update current message */
    if (new_msg != nullptr && !new_msg->empty()) {
      current_msg = new_msg;
      epoch_count = 0;
      logger.debug("Start flooding new message");
    }
    if (current_msg != nullptr && !current_msg->empty()) {
      /* If we have already send the message up to the limit, then set current message to null */
      if (epoch_count >= config.duplications) {
        current_msg = nullptr;
      }

      /* If we have sent the pdsch in the slot, then skip */
      if (phy_cfg.duplex.mode == srsran_duplex_mode_t::SRSRAN_DUPLEX_MODE_FDD && rx_slot_idx <= (last_sent_slot + 1)) {
        continue;
      } else if (rx_slot_idx == last_sent_slot) {
        continue;
      }
      /* Calculate the target slot index */
      uint32_t                   target_slot_idx = rx_slot_idx + config.delay_n_slots;
      GNBDLWorker::gnb_dl_task_t task            = {};
      task.rnti                                  = rnti;
      task.rnti_type                             = rnti_type;
      task.slot_idx                              = target_slot_idx;
      task.rx_tti                                = rx_slot_idx;
      task.rx_time                               = rx_timestamp;
      task.msg                                   = current_msg;
      task.mcs                                   = config.pdsch_mcs;
      task.prbs                                  = config.pdsch_prbs;
      /* Retrieve an available worker and set the task context */
      GNBDLWorker* w = (GNBDLWorker*)gnb_dl_pool.wait_worker(0);
      w->set_context(task);
      gnb_dl_pool.start_worker(w);
      last_sent_slot = rx_slot_idx;
      epoch_count++;
    }
  }
}

/* Apply the task to the ue_dl and gnb_ul */
void UETracker::work_on_task(const std::shared_ptr<Task>& task)
{
  auto now  = std::chrono::steady_clock::now();
  auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_message_time);
  if (diff.count() > config.close_timeout) {
    /* If the sniffer haven't received valid message for a long time, then deactivate the sniffer */
    logger.info("UETracker %s haven't received message for a long time, deactivate it %ld", name.c_str(), diff.count());
    deactivate();
    return;
  }

  /* Retrieve an available ue_dl worker */
  UEDLWorker* ue_dl_worker = (UEDLWorker*)ue_dl_pool.wait_worker(0);
  ue_dl_worker->set_task(task);
  ue_dl_pool.start_worker(ue_dl_worker);

  /* Retrieve an available gnb_ul worker */
  GNBULWorker* gnb_ul_worker = (GNBULWorker*)gnb_ul_pool.wait_worker(0);
  gnb_ul_worker->set_task(task);
  gnb_ul_pool.start_worker(gnb_ul_worker);
}

/* Update the last rx timestamp */
void UETracker::update_last_rx_timestamp()
{
  std::lock_guard<std::mutex> time_lock(time_mtx);
  auto                        now = std::chrono::steady_clock::now();
  if (now > last_message_time) {
    last_message_time = now;
  }
}