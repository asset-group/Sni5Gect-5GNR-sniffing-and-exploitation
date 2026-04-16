#ifndef SYNCER_H
#define SYNCER_H
#include "shadower/comp/trace_samples/trace_samples.h"
#include "shadower/source/source.h"
#include "shadower/utils/buffer_pool.h"
#include "shadower/utils/constants.h"
#include "shadower/utils/safe_queue.h"
#include "shadower/utils/task.h"
#include "srsran/common/threads.h"
#include "srsran/phy/phch/pbch_msg_nr.h"
#include "srsran/phy/sync/ssb.h"
#include "srsran/srslog/srslog.h"
#include <atomic>
#include <queue>
#include <thread>
#if ENABLE_CUDA
#include "shadower/comp/ssb/ssb_cuda.cuh"
#endif // ENABLE_CUDA

struct syncer_args_t {
  double                      srate;
  srsran_subcarrier_spacing_t scs;
  double                      dl_freq;
  double                      ssb_freq;
  srsran_ssb_pattern_t        pattern;
  srsran_duplex_mode_t        duplex_mode;
};

struct samples_t {
  uint32_t                            task_idx;
  uint32_t                            slot_idx;
  srsran_timestamp_t                  ts;
  std::shared_ptr<std::vector<cf_t> > dl_buffer[SRSRAN_MAX_CHANNELS];
  std::shared_ptr<std::vector<cf_t> > ul_buffer[SRSRAN_MAX_CHANNELS];
};

class Syncer : public srsran::thread
{
public:
  Syncer(syncer_args_t args_, Source* source_, ShadowerConfig& config_);
  ~Syncer() override = default;
  bool init();
  void stop();

  /* handler for cell found event */
  std::function<void(srsran_mib_nr_t&, uint32_t)> on_cell_found = [](srsran_mib_nr_t&, uint32_t) {};

  /* handler for push new slot to the task queue */
  std::function<void(std::shared_ptr<Task>&)> publish_subframe = [](std::shared_ptr<Task>&) {};

  /* handler for error event */
  std::function<void()> error_handler = []() {};

  /* retrieve tti and timestamp */
  void get_tti(uint32_t* idx, uint32_t* task_idx_, srsran_timestamp_t* ts);

  uint32_t     ncellid = 0;
  TraceSamples tracer_status;

private:
  srslog::basic_logger& logger = srslog::fetch_basic_logger("syncer", true);

  double       srate;
  uint32_t     sf_len;
  uint32_t     slot_per_sf;
  uint32_t     num_channels = 1;
  Source*      source       = nullptr;
  TraceSamples tracer_sib1;
#if ENABLE_CUDA
  SSBCuda* ssb_cuda = nullptr;
#endif // ENABLE_CUDA

  float    cfo_hz          = 0;
  uint32_t task_idx        = 0;
  uint32_t cur_task_idx    = 0;
  int32_t  samples_delayed = 0; /* Indicate if how many samples remaining in current slot */

  std::atomic<uint32_t>         tti{0};
  srsran_timestamp_t            timestamp_new{};
  srsran_timestamp_t            timestamp_prev{};
  syncer_args_t                 args         = {};
  srsran_ssb_t                  ssb          = {};
  srsran_mib_nr_t               mib          = {};
  srsran_pbch_msg_nr_t          pbch_msg     = {};
  srsran_csi_trs_measurements_t measurements = {};
  ShadowerConfig&               config;

  /* history queue, if the last subframe contains part of current subframe, then read from history queue */
  std::queue<std::shared_ptr<samples_t> > history_samples_queue;

  /* handler for increasing tti when receiving samples from source */
  void run_tti();

  /* listen to new subframes and keep getting sync */
  bool listen(std::shared_ptr<samples_t>& samples);

  /* run cell search to find the cell */
  bool run_cell_search();

  /* if lost sync, run sync find to get back to sync */
  bool run_sync_find(cf_t* buffer);

  /* is in sync, keep track of the ssb block and detect is lost sync */
  bool run_sync_track(cf_t* buffer);

  /* Unpack MIB information from PBCH message */
  bool handle_pbch(srsran_pbch_msg_nr_t& pbch_msg);

  /* Update sample offset and CFO for receiving samples next time */
  void handle_measurements(srsran_csi_trs_measurements_t& feedback);

  /* implement the thread class function to run the thread */
  void run_thread() override;

  std::unique_ptr<SharedBufferPool> buffer_pool = nullptr;

  std::mutex        time_mtx;
  std::atomic<bool> running{false};
  std::atomic<bool> cell_found{false};
  std::atomic<bool> in_sync{false};
};

#endif // SYNCER_H