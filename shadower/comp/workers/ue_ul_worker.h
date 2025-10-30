#pragma once
#include "shadower/source/source.h"
#include "shadower/utils/arg_parser.h"
#include "shadower/utils/constants.h"
#include "shadower/utils/utils.h"
#include "srsran/common/phy_cfg_nr.h"
#include "srsran/common/threads.h"
#include "srsran/mac/mac_rar_pdu_nr.h"
#include "srsran/srslog/logger.h"
#include <semaphore>

class UEULWorker : public srsran::thread
{
public:
  UEULWorker(srslog::basic_logger& logger_, ShadowerConfig& config_, Source* source_, srsue::nr::state& phy_state_);
  ~UEULWorker() override;

  struct ue_ul_task_t {
    uint32_t                               rnti;
    srsran_rnti_type_t                     rnti_type;
    uint32_t                               rx_slot_idx;
    srsran_timestamp_t                     rx_timestamp;
    std::shared_ptr<std::vector<uint8_t> > msg;
  };

  /* Initialize the UE UL worker  */
  bool init(srsran::phy_cfg_nr_t& phy_cfg_);

  /* Start the UE UL worker */
  void begin();

  /* Stop the UE UL worker */
  void stop();

  /* Update the UE UL configurations */
  bool update_cfg(srsran::phy_cfg_nr_t& phy_cfg_);

  /* Set the context of the UE UL worker */
  void set_context(std::shared_ptr<ue_ul_task_t> task_);

  /* Set PUSCH grant */
  int set_pusch_grant(srsran_dci_ul_nr_t& dci_ul, srsran_slot_cfg_t& slot_cfg);

  /* Set RAR grant */
  void set_ue_rar_grant(uint16_t                                                        rnti,
                        srsran_rnti_type_t                                              rnti_type,
                        std::array<uint8_t, srsran::mac_rar_subpdu_nr::UL_GRANT_NBITS>& rar_grant,
                        uint32_t                                                        slot_idx);

  void send_pusch(srsran_slot_cfg_t&                      slot_cfg,
                  std::shared_ptr<std::vector<uint8_t> >& pusch_payload,
                  srsran_sch_cfg_nr_t&                    pusch_cfg,
                  uint32_t                                rx_slot_idx,
                  srsran_timestamp_t&                     rx_timestamp);

  cf_t* buffer = nullptr;

private:
  void run_thread() override;

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

  srsran_slot_cfg_t             target_slot  = {};
  std::shared_ptr<ue_ul_task_t> current_task = nullptr;
};