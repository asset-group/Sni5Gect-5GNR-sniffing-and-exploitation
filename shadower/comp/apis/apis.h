#ifndef API_H
#define API_H
#include "srsran/config.h"
#include "srsran/phy/common/phy_common_nr.h"
#include "srsran/phy/common/timestamp.h"
#include "srsran/srslog/srslog.h"

struct __attribute__((packed)) uplink_api_hdr_t {
  uint32_t message_type = 1; // Uplink API message
  uint16_t rnti;
  uint16_t rnti_type;
  uint32_t slot_idx;
  uint32_t task_idx;
  uint32_t sf_len;
  uint32_t offset;
  float    snr_dB;
  time_t   full_secs;
  double   frac_secs;
  double   time_diff;
  uint32_t nof_prb;
  uint32_t start_symbol;
  uint32_t nof_symbol;
  bool     prb_map[SRSRAN_MAX_PRB_NR];
};

class APIs
{
public:
  APIs() = default;
  ~APIs();

  bool init(const char* zmq_address);

  bool is_initialized() const { return initialized; }

  int send_uplink_api_message(uplink_api_hdr_t& msg, cf_t* buffer, cf_t* last_buffer);

private:
  srslog::basic_logger& logger = srslog::fetch_basic_logger("APIs");

  bool  initialized = false;
  void* context     = nullptr;
  void* z_socket    = nullptr;
};

#endif // API_H