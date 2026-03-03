#ifndef GNB_DL_WORKER
#define GNB_DL_WORKER
#include "shadower/source/source.h"
#include "shadower/utils/arg_parser.h"
#include "shadower/utils/utils.h"
#include "srsran/common/phy_cfg_nr.h"
#include "srsran/common/thread_pool.h"
#include "srsran/common/threads.h"
#include "srsran/phy/gnb/gnb_dl.h"
#include "srsran/srslog/srslog.h"
#include <mutex>
#include <vector>

class GNBDLWorker : public srsran::thread_pool::worker
{
public:
  GNBDLWorker(srslog::basic_logger& logger_, Source* source_, ShadowerConfig& config_);
  ~GNBDLWorker() override;

  struct gnb_dl_task_t {
    uint16_t                               rnti;
    srsran_rnti_type_t                     rnti_type;
    uint32_t                               slot_idx;
    uint32_t                               rx_task_idx;
    uint32_t                               rx_tti;
    srsran_timestamp_t                     rx_time;
    uint32_t                               mcs  = 0;
    uint32_t                               prbs = 24;
    std::shared_ptr<std::vector<uint8_t> > msg;
  };

  // Initialize the gnb_dl worker
  bool init();

  // set the gnb_dl carrier
  bool update_cfg(srsran::phy_cfg_nr_t& phy_cfg_);

  // Set the context for the gnb_dl worker
  void set_context(gnb_dl_task_t& task_);

  // Encode pdsch message and send to UE
  bool send_pdsch();

  cf_t* tx_buffer     = nullptr;
  cf_t* gnb_dl_buffer = nullptr;

private:
  srslog::basic_logger& logger;
  std::mutex            mutex;
  ShadowerConfig&       config;
  srsran::phy_cfg_nr_t  phy_cfg = {};

  uint32_t sf_len        = 0;
  uint32_t slot_per_sf   = 1;
  uint32_t slot_len      = 0;
  uint32_t nof_sc        = 0;
  uint32_t nof_re        = 0;
  uint32_t numerology    = 0;
  double   slot_duration = 1e-3;

  Source*                source                 = nullptr;
  srsran_gnb_dl_t        gnb_dl                 = {};
  srsran_softbuffer_tx_t softbuffer_tx          = {};
  uint8_t*               data_tx[SRSRAN_MAX_TB] = {};

  // Current work information
  gnb_dl_task_t gnb_dl_task = {};

  // Worker implementation, send message to UE
  void work_imp() override;

  // Encode DCI UL message and send to UE
  bool send_dci_ul(uint32_t slot_idx);
};

#endif // GNB_DL_WORKER