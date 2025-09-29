#ifndef SNIFFER_HDR_CONFIG_H_
#define SNIFFER_HDR_CONFIG_H_
#include "srsran/common/band_helper.h"
#include "srsran/phy/common/phy_common_nr.h"
#include "srsran/srslog/srslog.h"
#include <cinttypes>
#include <fstream>
#include <iostream>
#include <string>
struct ChannelConfig {
  double rx_frequency = 3427.5e6;
  double tx_frequency = 3427.5e6;
  double rx_offset    = 0;
  double tx_offset    = 0;
  double rx_gain      = 40;
  double tx_gain      = 80;
  bool   enabled      = true;
};

struct ShadowerConfig {
  // Cell info
  uint16_t                    band;                  // Band number
  uint32_t                    nof_prb;               // Number of Physical Resource Blocks
  uint32_t                    offset_to_carrier = 0; // Frequency offset to apply to the carrier frequency (Hz)
  srsran_subcarrier_spacing_t scs_common;            // Subcarrier Spacing for common (kHz)
  srsran_subcarrier_spacing_t scs_ssb;               // Subcarrier Spacing for SSB (kHz)
  uint32_t                    ssb_period_ms;         // SSB periodicity in milliseconds
  uint32_t                    ssb_period;            // SSB periodicity in slots
  uint32_t                    dl_arfcn;              // Downlink ARFCN
  double                      dl_freq;               // Downlink frequency from ARFCN
  uint32_t                    ul_arfcn;              // Uplink ARFCN
  double                      ul_freq;               // Uplink frequency from ARFCN
  uint32_t                    ssb_arfcn;             // SSB ARFCN
  double                      ssb_freq;              // SSB frequency from ARFCN

  // Derived Cell configurations
  srsran_ssb_pattern_t ssb_pattern;
  srsran_duplex_mode_t duplex_mode;
  uint32_t             miu;
  uint32_t             slot_per_subframe;

  // RF info
  double   sample_rate;      // Sample rate (Hz)
  uint32_t nof_channels = 1; // Number of channels
  double   uplink_cfo;       // Uplink CFO correction for PUSCH decoding
  double   downlink_cfo;     // Downlink CFO correction for PDSCH decoding

  // Burst padding size
  uint32_t front_padding; // Number of samples to pad in front of the signal
  uint32_t back_padding;  // Number of samples to pad at the end of the signal

  // channels settings
  std::vector<ChannelConfig> channels;

  // Worker configurations
  size_t   pool_size = 24;  // Thread pool size
  uint32_t n_ue_dl_worker;  // Number of UE downlink workers
  uint32_t n_ue_ul_worker;  // Number of UE uplink workers
  uint32_t n_gnb_ul_worker; // Number of gNB uplink workers
  uint32_t n_gnb_dl_worker; // Number of gNB downlink workers

  // UE Tracker configurations
  uint32_t close_timeout;   // Close timeout, after how long haven't received a message should stop tracking the UE (ms)
  bool     parse_messages;  // Whether we should parse the messages or not
  uint32_t num_ues    = 10; // Number of UETrackers to pre-initialize
  bool     enable_gpu = false; // Enable GPU acceleration

  // Injector configurations
  uint32_t delay_n_slots;     // Number of slots to delay injecting the message
  uint32_t duplications;      // Number of duplications to send in each inject
  float    tx_cfo_correction; // Uplink CFO correction (Hz)
  int32_t  tx_advancement;    // Number of samples to send in advance, so that on the receiver side, it arrives at the
  uint32_t pdsch_mcs;         // PDSCH MCS used for injection
  uint32_t pdsch_prbs;        // PDSCH PRBs used for injection
  uint32_t ul_advancement;
  uint32_t ul_channel;

  // Source configurations
  std::string source_type;   // Source type: file, uhd, limeSDR
  std::string source_params; // Source parameters, e.g., device args, record file
  std::string source_module; // Source module file

  // Recorder configurations
  bool enable_recorder = false; // Enable recording the IQ samples to subframes

  // Logger configurations
  srslog::basic_levels log_level        = srslog::basic_levels::info;
  srslog::basic_levels bc_worker_level  = srslog::basic_levels::info;
  srslog::basic_levels worker_log_level = srslog::basic_levels::info;
  srslog::basic_levels syncer_log_level = srslog::basic_levels::info;

  // Pcap folder
  std::string pcap_folder = "logs/";

  // Exploit module to load
  std::string exploit_module;
};

int parse_args(ShadowerConfig& config, int argc, char* argv[]);
#endif // SNIFFER_HDR_CONFIG_H_