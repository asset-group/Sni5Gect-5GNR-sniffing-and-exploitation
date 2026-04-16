#ifndef WD_WORKER_H
#define WD_WORKER_H
#include "shadower/comp/workers/ws_filter.h"
#include "shadower/modules/exploit.h"
#include "shadower/utils/constants.h"
#include "srsran/phy/common/phy_common_nr.h"
#include "srsran/srslog/srslog.h"
#include <atomic>
#include <epan/timestamp.h>
#include <mutex>
#include <span>
#include <string>
#include <vector>
typedef struct epan_session     epan_t;
typedef struct epan_dissect     epan_dissect_t;
typedef struct epan_column_info column_info;

/* Use Wireshark's standard first 7 columns so COL_INFO is available by default. */
constexpr int      wireshark_num_columns   = 7;
constexpr uint32_t epan_protocol_max_len   = 128;
constexpr uint32_t epan_summary_max_len    = 2048;
constexpr uint16_t fake_udp_source_port_nr = 0xe7a6;
constexpr uint16_t fake_udp_dest_port_nr   = 9999;
constexpr uint32_t fake_ip_len_offset      = 16;
constexpr uint32_t fake_udp_len_offset     = 38;
constexpr uint32_t mac_nr_sig_offset       = 42;
constexpr uint32_t mac_nr_sig_len          = 6;

class WDWorker
{
public:
  WDWorker(srsran_duplex_mode_t duplex_mode, srslog::basic_levels log_level);
  ~WDWorker();

  /* Initialize wireshark and epan dissector */
  int initialize();

  /* Clean up the wireshark dissector */
  void cleanup();

  /* Run dissection, generate summary and match filters */
  void process(uint8_t*    data,
               uint32_t    len,
               uint16_t    rnti,
               uint16_t    frame_number,
               uint16_t    slot_number,
               uint32_t    slot_idx,
               direction_t direction,
               Exploit*    exploit);

private:
  int  initialize_locked();
  void cleanup_locked();

  void normalize_ip_udp_headers();

  srslog::basic_logger& logger = srslog::fetch_basic_logger("WDWorker");
  srsran_duplex_mode_t  duplex_mode;

  const std::string    wireshark_env_prefix                        = "WIRESHARK";
  static constexpr int wireshark_column_entries                    = 6;
  const char* const    wireshark_columns[wireshark_column_entries] = {"No.", "%m", "Time", "%t", "Info", "%i"};

  epan_t*         epan_ptr         = nullptr;
  epan_dissect_t* epan_dissect_ptr = nullptr;

  column_info*         cinfo = nullptr;
  std::atomic<size_t>  frame_counter{0};
  std::atomic<bool>    initialized{false};
  std::vector<uint8_t> buffer;
  std::mutex           worker_mutex;
};

#endif // WD_WORKER_H