#ifndef BROADCAST_WORKER_H
#define BROADCAST_WORKER_H
#include "shadower/utils/arg_parser.h"
#include "shadower/utils/constants.h"
#include "shadower/utils/task.h"
#include "shadower/utils/utils.h"
#include "srsran/mac/mac_rar_pdu_nr.h"
#include "srsran/srslog/srslog.h"
#include "srsue/hdr/phy/nr/state.h"
#include <atomic>
#if ENABLE_CUDA
#include "shadower/comp/fft/fft_processor.cuh"
#endif // ENABLE_CUDA
class BroadCastWorker
{
public:
  explicit BroadCastWorker(ShadowerConfig& config_);
  ~BroadCastWorker() = default;

  /* Look for SIB1 if not already found, else search for RACH msg2 */
  bool work(const std::shared_ptr<Task>& task);

  /* Apply the configuration from MIB */
  bool apply_config_from_mib(srsran_mib_nr_t& mib_, uint32_t ncellid_);

  /* Apply the configuration from SIB1 */
  bool apply_config_from_sib1(asn1::rrc_nr_r17::sib1_s& sib1_);

  void set_rnti(uint16_t rnti_, srsran_rnti_type_t rnti_type_)
  {
    rnti      = rnti_;
    rnti_type = rnti_type_;
  }

  /* handler function to create new UE tracker when new RACH msg2 is found */
  std::function<void(uint16_t, std::array<uint8_t, 27UL>&, uint32_t, uint32_t)> on_ue_found =
      [](uint16_t, std::array<uint8_t, 27UL>&, uint32_t, uint32_t) {};

  /* handler function to apply the configuration from SIB1 */
  std::function<void(asn1::rrc_nr_r17::sib1_s&)> on_sib1_found = [](asn1::rrc_nr_r17::sib1_s&) {};

  srsran_mib_nr_t          mib  = {}; // Master Information Block configuration
  asn1::rrc_nr_r17::sib1_s sib1 = {}; // System Information Block 1 configuration

private:
  srslog::basic_logger& logger = srslog::fetch_basic_logger("BCW", false);
  // For handling creating new UE tracker
  ShadowerConfig&      config;
  srsran::phy_cfg_nr_t phy_cfg     = {}; // Physical configuration
  srsue::nr::state     phy_state   = {}; // Physical state to track grants
  srsran_slot_cfg_t    slot_cfg    = {};
  srsran_sch_cfg_nr_t  pdsch_cfg   = {}; // PDSCH configuration
  srsran_ue_dl_nr_t    ue_dl       = {}; // UE DL instance to do DCI search and PDSCH decoding
  uint32_t             ncellid     = 0;  // Physical cell ID
  uint32_t             pid         = 0;
  uint32_t             sf_len      = 0;
  uint32_t             slot_len    = 0;
  uint32_t             slot_per_sf = 1;
  cf_t*                rx_buffer   = nullptr; // Input buffer for PDSCH decoding
  uint16_t             rnti        = 0xffff;  // RNTI for broadcasting
  srsran_rnti_type_t   rnti_type   = srsran_rnti_type_si;

#if ENABLE_CUDA
  FFTProcessor* fft_processor = nullptr;
#endif // ENABLE_CUDA

  srsran_harq_ack_resource_t ack_resource  = {};
  srsran_softbuffer_rx_t     softbuffer_rx = {};

  // decode PDSCH symbols into bytes
  bool pdsch_decode(uint32_t slot_idx, uint32_t task_idx);
  // use asn1 to decode bytes into SIB1 message
  bool decode_sib1(srsran::unique_byte_buffer_t& data);
  // use asn1 to decode bytes into RAR message
  bool decode_rar(srsran::unique_byte_buffer_t& data, uint32_t slot_idx, uint32_t task_idx);
};

#endif // BROADCAST_WORKER_H