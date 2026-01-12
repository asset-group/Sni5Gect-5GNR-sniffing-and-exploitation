#ifndef GNB_UL_WORKER
#define GNB_UL_WORKER
#include "shadower/comp/trace_samples/trace_samples.h"
#include "shadower/comp/workers/wd_worker.h"
#include "shadower/modules/exploit.h"
#include "shadower/utils/arg_parser.h"
#include "shadower/utils/task.h"
#include "shadower/utils/utils.h"
#include "srsran/common/mac_pcap.h"
#include "srsran/common/phy_cfg_nr.h"
#include "srsran/common/thread_pool.h"
#include "srsran/phy/gnb/gnb_ul.h"
#include "srsue/hdr/phy/nr/state.h"
#if ENABLE_CUDA
#include "shadower/comp/fft/fft_processor.cuh"
#endif // ENABLE_CUDA
class GNBULWorker : public srsran::thread_pool::worker
{
public:
  GNBULWorker(srslog::basic_logger&             logger_,
              ShadowerConfig&                   config_,
              srsue::nr::state&                 phy_state_,
              WDWorker*                         wd_worker_,
              Exploit*                          exploit_,
              std::shared_ptr<srsran::mac_pcap> pcap_writer_);
  ~GNBULWorker() override;

  /* Initialize the GNB DL worker */
  bool init(srsran::phy_cfg_nr_t& phy_cfg_);

  /* Update the GNB DL configurations */
  bool update_cfg(srsran::phy_cfg_nr_t& phy_cfg_);

  /* update the rnti */
  void set_rnti(uint16_t rnti_, srsran_rnti_type_t rnti_type_);

  /* Set current task for the gnb_ul worker */
  void set_task(std::shared_ptr<Task> task_);

  /* Update the last received message timestamp */
  std::function<void()> update_rx_timestamp = []() {};

  /* Update the number of samples to send in advance */
  void set_ta_samples(double ta_time)
  {
    int new_ta_samples = ta_time * srate;
    if (new_ta_samples < 0) {
      new_ta_samples = 0;
    }
    ta_samples = new_ta_samples;
    logger.info("Setting Timing Advance samples for %u to %d", rnti, ta_samples);
  }

  void set_ta_samples(int32_t new_ta_samples)
  {
    if (new_ta_samples < 0) {
      new_ta_samples = 0;
    }
    ta_samples = new_ta_samples;
    logger.info("Setting Timing Advance samples for %u to %d", rnti, ta_samples);
  }

  /* Process the tasks */
  void process_task(std::shared_ptr<Task> task);

private:
  srslog::basic_logger&             logger;
  std::mutex                        mutex;
  ShadowerConfig&                   config;
  srsue::nr::state&                 phy_state;
  std::shared_ptr<srsran::mac_pcap> pcap_writer;
  srsran::phy_cfg_nr_t              phy_cfg = {};
  static TraceSamples               tracer_ul_pusch;
#if ENABLE_CUDA
  FFTProcessor* fft_processor = nullptr;
#endif // ENABLE_CUDA

  double             srate          = 0;
  uint32_t           sf_len         = 0;
  uint32_t           slot_per_sf    = 1;
  uint32_t           slot_per_frame = 1;
  uint32_t           slot_len       = 0;
  uint32_t           nof_sc         = 0;
  uint32_t           nof_re         = 0;
  uint32_t           numerology     = 0;
  uint32_t           pid            = 0;
  uint16_t           rnti           = SRSRAN_INVALID_RNTI;
  srsran_rnti_type_t rnti_type      = srsran_rnti_type_c;
  uint32_t           ta_samples; // Number of samples in advance according to timing advance command

  cf_t*                  buffer        = nullptr;
  WDWorker*              wd_worker     = nullptr;
  Exploit*               exploit       = nullptr;
  srsran_gnb_ul_t        gnb_ul        = {};
  srsran_sch_cfg_nr_t    pusch_cfg     = {};
  srsran_softbuffer_rx_t softbuffer_rx = {};
  std::shared_ptr<Task>  task          = nullptr; // Current specified task

  /* Worker implementation, decode message send from UE to base station */
  void work_imp() override;

  /* Handle PUSCH decoding */
  void handle_pusch(srsran_slot_cfg_t& slot_cfg);
};
#endif // GNB_UL_WORKER