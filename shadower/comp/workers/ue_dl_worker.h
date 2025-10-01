#ifndef UE_DL_WORKER
#define UE_DL_WORKER
#include "shadower/comp/trace_samples/trace_samples.h"
#include "shadower/comp/workers/wd_worker.h"
#include "shadower/modules/exploit.h"
#include "shadower/utils/arg_parser.h"
#include "shadower/utils/phy_cfg_utils.h"
#include "shadower/utils/task.h"
#include "shadower/utils/ue_dl_utils.h"
#include "srsran/asn1/rrc_nr.h"
#include "srsran/common/mac_pcap.h"
#include "srsran/common/phy_cfg_nr.h"
#include "srsran/common/thread_pool.h"
#include "srsran/mac/mac_sch_pdu_nr.h"
#include "srsran/phy/common/phy_common_nr.h"
#include "srsue/hdr/phy/nr/state.h"
#if ENABLE_CUDA
#include "shadower/comp/fft/fft_processor.cuh"
#endif // ENABLE_CUDA
class UEDLWorker : public srsran::thread_pool::worker
{
public:
  UEDLWorker(srslog::basic_logger&             logger_,
             ShadowerConfig&                   config_,
             srsue::nr::state&                 phy_state_,
             WDWorker*                         wd_worker_,
             Exploit*                          exploit_,
             std::shared_ptr<srsran::mac_pcap> pcap_writer_);
  ~UEDLWorker() override;

  /* Initialize the UE DL worker */
  bool init(srsran::phy_cfg_nr_t& phy_cfg_);

  /* Update the UE DL configurations */
  bool update_cfg(srsran::phy_cfg_nr_t& phy_cfg_);

  /* Update the pending RRC setup */
  void update_pending_rrc_setup(bool pending_rrc_setup_);

  /* Update the rnti */
  void set_rnti(uint16_t rnti_, srsran_rnti_type_t rnti_type_);

  /* Set current task for the ue_dl worker */
  void set_task(std::shared_ptr<Task> task_);

  /* Process subframe */
  void process_task(std::shared_ptr<Task> task_);

  /* On UE disconnection, also deactivate the UE tracker */
  std::function<void()> deactivate = [] {};

  /* Apply the configuration to cell group config */
  std::function<void(asn1::rrc_nr::cell_group_cfg_s&)> apply_cell_group_cfg = [](asn1::rrc_nr::cell_group_cfg_s&) {};

  /* Update the last received message timestamp */
  std::function<void()> update_rx_timestamp = []() {};

  /* Update timing advance command */
  std::function<void(int32_t)> update_timing_advance = [](int32_t) {};

  /* Call back for DCI UL found */
  std::function<int(srsran_dci_ul_nr_t& dci_ul, srsran_slot_cfg_t& slot_cfg)> on_dci_ul_found =
      [](srsran_dci_ul_nr_t&, srsran_slot_cfg_t&) { return -1; };

private:
  srslog::basic_logger&             logger;
  std::mutex                        mutex;
  ShadowerConfig&                   config;
  srsue::nr::state&                 phy_state;
  std::shared_ptr<srsran::mac_pcap> pcap_writer;
  srsran::phy_cfg_nr_t              phy_cfg = {};
  static TraceSamples               tracer_dl_pdsch;  // DCI DL + PDSCH
  static TraceSamples               tracer_dl_dci_ul; // DCI UL

#if ENABLE_CUDA
  FFTProcessor* fft_processor = nullptr;
#endif // ENABLE_CUDA

  double             srate             = 0;
  uint32_t           sf_len            = 0;
  uint32_t           slot_per_sf       = 1;
  uint32_t           slot_per_frame    = 1;
  uint32_t           slot_len          = 0;
  uint32_t           nof_sc            = 0;
  uint32_t           nof_re            = 0;
  uint32_t           numerology        = 0;
  uint32_t           pid               = 0;
  bool               pending_rrc_setup = true;
  uint16_t           rnti              = SRSRAN_INVALID_RNTI;
  srsran_rnti_type_t rnti_type         = srsran_rnti_type_c;

  cf_t*                      buffer        = nullptr;
  WDWorker*                  wd_worker     = nullptr;
  Exploit*                   exploit       = nullptr;
  srsran_ue_dl_nr_t          ue_dl         = {};
  srsran_sch_cfg_nr_t        pdsch_cfg     = {};
  srsran_softbuffer_rx_t     softbuffer_rx = {};
  srsran_harq_ack_resource_t ack_resource  = {};
  std::shared_ptr<Task>      task          = nullptr; // Current specified task

  /* Worker implementation, decode message send from base station to UE */
  void work_imp() override;

  /* Decode the PDSCH message */
  void handle_pdsch(srsran_slot_cfg_t& slot_cfg);

  /* Decode DL-SCH message */
  void handle_dlsch(uint8_t* sdu, uint32_t len);

  /* Handle CCCH message */
  void handle_dl_ccch(uint8_t* sdu, uint32_t len);
};

#endif // UE_DL_WORKER