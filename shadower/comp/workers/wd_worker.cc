#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "shadower/comp/workers/wd_worker.h"
#include "shadower/comp/workers/ws_filter.h"
#include "shadower/utils/utils.h"
#include <cstring>
#include <epan/column-info.h>
#include <epan/column-utils.h>
#include <epan/column.h>
#include <epan/dfilter/dfilter.h>
#include <epan/epan.h>
#include <epan/epan_dissect.h>
#include <epan/packet.h>
#include <epan/prefs.h>
#include <epan/proto.h>
#include <epan/register.h>
#include <utility>
#include <wiretap/wtap.h>
#include <wsutil/wslog.h>

namespace {
std::once_flag ws_log_init_once;

void initialize_wireshark_logging_once()
{
  ws_log_init(nullptr);
  ws_log_set_level(LOG_LEVEL_WARNING);
}
} // namespace

uint16_t read_be16(const uint8_t* data)
{
  return static_cast<uint16_t>((static_cast<uint16_t>(data[0]) << 8) | data[1]);
}

void write_be16(uint8_t* data, uint16_t value)
{
  data[0] = static_cast<uint8_t>((value >> 8) & 0xff);
  data[1] = static_cast<uint8_t>(value & 0xff);
}

const nstime_t* get_frame_ts(struct packet_provider_data* /*prov*/, uint32_t /*frame_num*/)
{
  static nstime_t empty{};
  return &empty;
}

const packet_provider_funcs packet_provider = {
    get_frame_ts,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
};

WDWorker::WDWorker(srsran_duplex_mode_t _duplex_mode, srslog::basic_levels log_level) : duplex_mode(_duplex_mode)
{
  logger.set_level(log_level);
  buffer.resize(4096);
  if (initialize() != 0) {
    throw std::runtime_error("Failed to initialize WDWorker");
  }
}

int WDWorker::initialize()
{
  std::lock_guard<std::mutex> lock(worker_mutex);

  return initialize_locked();
}

int WDWorker::initialize_locked()
{
  if (initialized.load()) {
    return 0; // Already initialized
  }

  std::call_once(ws_log_init_once, initialize_wireshark_logging_once);

  /* Set the timestamp options */
  timestamp_set_type(TS_RELATIVE);
  timestamp_set_precision(TS_PREC_AUTO);
  timestamp_set_seconds_type(TS_SECONDS_DEFAULT);
  wtap_init(false);

  /* Initialize epan */
  if (!epan_init(nullptr, nullptr, false)) {
    logger.error("Failed to initialize epan");
    wtap_cleanup();
    return -1;
  }
  epan_load_settings();
  prefs_apply_all();
  dissector_handle_t mac_nr_framed = find_dissector("mac-nr-framed");
  if (mac_nr_framed == nullptr) {
    logger.error("Failed to find mac-nr-framed dissector");
    wtap_cleanup();
    return -1;
  }
  /* Register the fake UDP ports for the mac-nr dissector */
  dissector_add_uint("udp.port", fake_udp_source_port_nr, mac_nr_framed);
  dissector_add_uint("udp.port", fake_udp_dest_port_nr, mac_nr_framed);

  /* Create epan instances */
  epan_ptr = epan_new(nullptr, &packet_provider);
  if (epan_ptr == nullptr) {
    logger.error("Failed to create epan instance");
    wtap_cleanup();
    return -1;
  }

  /* Create epan_dissect instance */
  epan_dissect_ptr = epan_dissect_new(epan_ptr, true, false);
  if (epan_dissect_ptr == nullptr) {
    logger.error("Failed to create epan_dissect instance");
    epan_free(epan_ptr);
    wtap_cleanup();
    return -1;
  }

  cinfo = g_new0(column_info, 1);
  if (cinfo == nullptr) {
    logger.error("Failed to allocate column_info");
    epan_dissect_free(epan_dissect_ptr);
    epan_dissect_ptr = nullptr;
    epan_free(epan_ptr);
    epan_ptr = nullptr;
    wtap_cleanup();
    return -1;
  }

  build_column_format_array(cinfo, wireshark_num_columns, true);
  initialized = true;
  return 0; // Success
}

WDWorker::~WDWorker()
{
  cleanup();
}

void WDWorker::cleanup()
{
  std::lock_guard<std::mutex> lock(worker_mutex);

  cleanup_locked();
}

void WDWorker::cleanup_locked()
{
  if (cinfo != nullptr) {
    col_cleanup(cinfo);
    g_free(cinfo);
    cinfo = nullptr;
  }

  if (epan_ptr != nullptr) {
    epan_free(epan_ptr);
    epan_ptr = nullptr;
  }

  if (epan_dissect_ptr != nullptr) {
    epan_dissect_cleanup(epan_dissect_ptr);
    g_free(epan_dissect_ptr);
    epan_dissect_ptr = nullptr;
  }

  epan_cleanup();
  wtap_cleanup();
  initialized = false;
}

void WDWorker::normalize_ip_udp_headers()
{
  if (buffer.size() < (mac_nr_sig_offset + mac_nr_sig_len)) {
    return;
  }

  static constexpr char mac_nr_signature[] = "mac-nr";
  if (memcmp(buffer.data() + mac_nr_sig_offset, mac_nr_signature, mac_nr_sig_len) != 0) {
    return;
  }

  buffer.erase(buffer.begin() + mac_nr_sig_offset, buffer.begin() + mac_nr_sig_offset + mac_nr_sig_len);
  if (buffer.size() > (fake_udp_len_offset + 2)) {
    /* Update the UDP length field */
    uint16_t udp_len = read_be16(buffer.data() + fake_udp_len_offset);
    if (udp_len >= mac_nr_sig_len) {
      write_be16(buffer.data() + fake_udp_len_offset, static_cast<uint16_t>(udp_len - mac_nr_sig_len));
    }
  }

  if (buffer.size() > (fake_ip_len_offset + 2)) {
    /* Update the IP length field */
    uint16_t ip_len = read_be16(buffer.data() + fake_ip_len_offset);
    if (ip_len >= mac_nr_sig_len) {
      write_be16(buffer.data() + fake_ip_len_offset, static_cast<uint16_t>(ip_len - mac_nr_sig_len));
    }
  }
}

/* Call wireshark epan to dissect the packet, generate the packet summary and match the filters */
void WDWorker::process(uint8_t*    data,
                       uint32_t    len,
                       uint16_t    rnti,
                       uint16_t    frame_number,
                       uint16_t    slot_number,
                       uint32_t    slot_idx,
                       direction_t direction,
                       Exploit*    exploit)
{
  std::lock_guard<std::mutex> lock(worker_mutex);

  if (data == nullptr || len == 0) {
    return;
  }

  // A single EPAN session/dissector context is shared in WDWorker,
  // so process calls must be serialized across UL/DL worker threads.
  if (!initialized.load() && initialize_locked() != 0) {
    logger.error("WDWorker is not initialized");
    return;
  }

  constexpr size_t header_slack = 128;
  size_t           min_capacity = static_cast<size_t>(len) + header_slack;
  if (buffer.size() < min_capacity) {
    buffer.resize(min_capacity);
  }

  /* Add the fake udp header for DLT-149 UDP*/
  int packet_len = add_fake_header(buffer.data(), data, len, rnti, frame_number, slot_number, direction, duplex_mode);
  if (packet_len <= 0) {
    logger.error("Failed to build framed packet");
    return;
  }

  buffer.resize(static_cast<size_t>(packet_len));

  /* Check the ip and udp headers */
  normalize_ip_udp_headers();
  packet_len = static_cast<int>(buffer.size());

  /* Run the pre-dissection of the exploit module to get the filters */
  if (exploit != nullptr) {
    exploit->pre_dissection();
    for (const auto& filter : exploit->filters) {
      filter->match = false;
    }
    for (const auto& field : exploit->fields) {
      field->reset_extracted_values();
    }
  }

  /* Initialize the packet */
  wtap_rec   rec;
  frame_data fd_local;
  wtap_rec_init(&rec, packet_len);
  wtap_setup_packet_rec(&rec, WTAP_ENCAP_ETHERNET);
  rec.rec_header.packet_header.caplen = packet_len;
  rec.rec_header.packet_header.len    = packet_len;
  rec.presence_flags                  = WTAP_HAS_TS | WTAP_HAS_CAP_LEN;
  ws_buffer_append(&rec.data, buffer.data(), packet_len);
  frame_data_init(&fd_local, ++frame_counter, &rec, 0, 0);

  /* Register the filter */
  if (exploit != nullptr) {
    for (const auto& filter : exploit->filters) {
      if (filter->compiled && filter->compiled_filter != nullptr) {
        epan_dissect_prime_with_dfilter(epan_dissect_ptr, filter->compiled_filter);
      }
    }

    for (const auto& field : exploit->fields) {
      if (!field->compiled) {
        continue;
      }

      for (int hf_id : field->hf_ids) {
        epan_dissect_prime_with_hfid(epan_dissect_ptr, hf_id);
      }
    }
  }

  /* Run the dissection */
  col_custom_prime_edt(epan_dissect_ptr, cinfo);
  epan_dissect_run(epan_dissect_ptr, WTAP_FILE_TYPE_SUBTYPE_UNKNOWN, &rec, &fd_local, cinfo);
  epan_dissect_fill_in_columns(epan_dissect_ptr, false, true);

  /* Get the packet summary */
  column_info* active_cinfo = (epan_dissect_ptr->pi.cinfo != nullptr) ? epan_dissect_ptr->pi.cinfo : cinfo;
  const char*  info_col     = col_get_text(active_cinfo, COL_INFO);
  const char*  safe_info    = nullptr;
  if (info_col != nullptr && info_col[0] != '\0') {
    safe_info = info_col;
  } else {
    safe_info = "Unknown";
  }

  if (direction == DL) {
    logger.info(GREEN "%u [S:%u] --> %s" RESET, rnti, slot_idx, safe_info);
  } else {
    logger.info(BLUE "%u [S:%u] <-- %s" RESET, rnti, slot_idx, safe_info);
  }

  if (exploit != nullptr) {
    /* Update if the filters match */
    for (const auto& filter : exploit->filters) {
      if (filter->compiled_filter != nullptr) {
        bool filter_match = false;
        dfilter_load_field_references_edt(filter->compiled_filter, epan_dissect_ptr);
        filter_match  = dfilter_apply_edt(filter->compiled_filter, epan_dissect_ptr);
        filter->match = filter_match;
      }
    }

    for (const auto& field : exploit->fields) {
      switch (field->field_type) {
        case FIELD_UINT32: {
          uint32_t value;
          if (field->read_first_uint32_field(epan_dissect_ptr)) {
            field->has_uint32 = true;
          }
          break;
        }
        case FIELD_STRING: {
          std::string value;
          if (field->read_first_string_field(epan_dissect_ptr)) {
            field->has_string = true;
          }
          break;
        }
        default:
          break;
      }
    }

    /* Run the post-dissection of the exploit module */
    exploit->post_dissection(buffer.data(), packet_len, data, len, direction, slot_idx, logger);
  }

  /* Cleanup after dissection */
  frame_data_destroy(&fd_local);
  epan_dissect_reset(epan_dissect_ptr);
  wtap_rec_cleanup(&rec);
}
