#pragma once
#include "shadower/source/source.h"
#include "shadower/utils/arg_parser.h"
#include "shadower/utils/constants.h"
#include "shadower/utils/utils.h"
#include "srsran/common/phy_cfg_nr.h"
#include "srsran/common/thread_pool.h"
#include "srsran/srslog/logger.h"
#include <semaphore>

class UEULWorker : public srsran::thread_pool::worker
{
public:
  UEULWorker(srslog::basic_logger& logger_, ShadowerConfig& config_, Source* source_, srsue::nr::state& phy_state_);
  ~UEULWorker() override;

  /* Initialize the UE UL worker  */
  bool init(srsran::phy_cfg_nr_t& phy_cfg_);

  /* Update the UE UL configurations */
  bool update_cfg(srsran::phy_cfg_nr_t& phy_cfg_);

  /* Set PUSCH grant */
  int set_pusch_grant(srsran_dci_ul_nr_t& dci_ul, srsran_slot_cfg_t& slot_cfg);

  void send_pusch(srsran_slot_cfg_t&                      slot_cfg,
                  std::shared_ptr<std::vector<uint8_t> >& pusch_payload,
                  uint32_t                                rx_slot_idx,
                  srsran_timestamp_t&                     rx_timestamp,
                  srsran_dci_ul_nr_t&                     dci_ul);

  cf_t* buffer = nullptr;

private:
  void work_imp() override;

  srslog::basic_logger&   logger;
  Source*                 source;
  bool                    grant_available = false;
  std::mutex              mutex;
  std::condition_variable cv;
  std::atomic<bool>       running{true};

  ShadowerConfig&        config;
  srsue::nr::state&      phy_state;
  srsran::phy_cfg_nr_t   phy_cfg       = {};
  srsran_ue_ul_nr_t      ue_ul         = {};
  srsran_softbuffer_tx_t softbuffer_tx = {};

  double   srate         = 0;
  uint32_t sf_len        = 0;
  uint32_t slot_len      = 0;
  uint32_t slot_per_sf   = 0;
  uint32_t nof_sc        = 0;
  uint32_t nof_re        = 0;
  double   slot_duration = SF_DURATION;
};