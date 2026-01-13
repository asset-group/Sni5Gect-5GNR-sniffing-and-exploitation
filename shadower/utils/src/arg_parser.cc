#include "shadower/utils/arg_parser.h"
#include "shadower/utils/constants.h"
#include <iostream>
#include <yaml-cpp/yaml.h>

template <typename T>
static T node_as(const YAML::Node& n, const std::string& key, const T& def)
{
  if (!n) {
    return def;
  }
  if (!n[key]) {
    return def;
  }
  try {
    return n[key].as<T>();
  } catch (...) {
    return def;
  }
}

int parse_args(ShadowerConfig& config, int argc, char* argv[])
{
  if (argc < 2) {
    std::cerr << "Usage: " << argv[0] << " <config_file>" << std::endl;
    return SRSRAN_ERROR;
  }
  char*      config_file = argv[1];
  YAML::Node root;
  try {
    root = YAML::LoadFile(config_file);
  } catch (const YAML::BadFile& e) {
    std::cerr << "Error: cannot open config file: " << config_file << std::endl;
    return SRSRAN_ERROR;
  } catch (const YAML::Exception& e) {
    std::cerr << "YAML parse error: " << e.what() << std::endl;
    return SRSRAN_ERROR;
  }

  /* --- cell --- */
  if (!root["cell"]) {
    std::cerr << "Please provide cell configuration" << std::endl;
    return SRSRAN_ERROR;
  }
  auto cell                = root["cell"];
  config.band              = node_as<uint16_t>(cell, "band", 78);
  config.nof_prb           = node_as<uint32_t>(cell, "nof_prb", 51);
  config.offset_to_carrier = node_as<uint32_t>(cell, "offset_to_carrier", 0);

  srsran::srsran_band_helper band_helper;

  // Convert the subcarrier spacing from string
  std::string scs_common_str = node_as<std::string>(cell, "scs_common", "30");
  config.scs_common          = srsran_subcarrier_spacing_from_str(scs_common_str.c_str());
  std::string scs_ssb_str    = node_as<std::string>(cell, "scs_ssb", "30");
  config.scs_ssb             = srsran_subcarrier_spacing_from_str(scs_ssb_str.c_str());

  // Convert DL arfcn to frequency
  config.dl_arfcn = node_as<uint32_t>(cell, "dl_arfcn", 0);
  assert(0 != config.dl_arfcn);
  config.dl_freq = band_helper.nr_arfcn_to_freq(config.dl_arfcn);
  std::cout << "DL arfcn: " << config.dl_arfcn << std::endl;
  std::cout << "DL frequency: " << config.dl_freq / 1e6 << " MHz" << std::endl;

  // Derive the uplink arfcn and frequency from downlink
  config.ul_arfcn = band_helper.get_ul_arfcn_from_dl_arfcn(config.dl_arfcn);
  config.ul_freq  = band_helper.nr_arfcn_to_freq(config.ul_arfcn);
  std::cout << "UL arfcn: " << config.ul_arfcn << std::endl;
  std::cout << "UL frequency: " << config.ul_freq / 1e6 << " MHz" << std::endl;

  // Convert SSB arfcn to frequency
  config.ssb_arfcn = node_as<uint32_t>(cell, "ssb_arfcn", 0);
  config.ssb_freq  = band_helper.nr_arfcn_to_freq(config.ssb_arfcn);
  std::cout << "SSB arfcn: " << config.ssb_arfcn << std::endl;
  std::cout << "SSB frequency: " << config.ssb_freq / 1e6 << " MHz" << std::endl;

  // Get the point A frequency
  double frequency_pointA = band_helper.get_abs_freq_point_a_from_center_freq(config.nof_prb, config.dl_freq);
  std::cout << "Frequency point A: " << frequency_pointA / 1e6 << " MHz" << std::endl;

  // Derive other cell configurations
  config.ssb_pattern       = band_helper.get_ssb_pattern(config.band, config.scs_ssb);
  config.duplex_mode       = band_helper.get_duplex_mode(config.band);
  config.ssb_period_ms     = node_as<uint32_t>(cell, "ssb_period_ms", 10);
  config.ssb_period        = node_as<uint32_t>(cell, "ssb_period", 0);
  config.miu               = 1 << config.scs_ssb;
  config.slot_per_subframe = 1 << config.scs_common;
  if (config.ssb_period == 0) {
    config.ssb_period = config.ssb_period_ms * config.slot_per_subframe;
  }

  /* source */
  if (!root["source"]) {
    std::cerr << "Please provide source configuration" << std::endl;
    return SRSRAN_ERROR;
  }
  auto source          = root["source"];
  config.source_type   = node_as<std::string>(source, "source_type", "file");
  config.source_params = node_as<std::string>(source, "source_params", "");
  assert(!config.source_type.empty());
  assert(!config.source_params.empty());
  if (config.source_type == "file") {
    config.source_module = file_source_module_path;
  } else if (config.source_type == "uhd") {
    config.source_module = uhd_source_module_path;
  } else if (config.source_type == "limeSDR") {
    config.source_module = limesdr_source_module_path;
  } else {
    config.source_module = node_as<std::string>(source, "source_module", "");
  }
  std::cout << "Source type: " << config.source_type << std::endl;
  std::cout << "Source params: " << config.source_params << std::endl;
  std::cout << "Source module: " << config.source_module << std::endl;

  /* --- enable recorder or not --- */
  config.enable_recorder = node_as<bool>(root, "enable_recorder", false);
  std::cout << "Enable recorder: " << (config.enable_recorder ? "true" : "false") << std::endl;
  config.pcap_folder = node_as<std::string>(root, "pcap_folder", "logs/");
  std::cout << "Pcap folder: " << config.pcap_folder << std::endl;

  /* --- rf --- */
  if (!root["rf"]) {
    std::cerr << "Please provide rf configuration" << std::endl;
    return SRSRAN_ERROR;
  }
  auto rf             = root["rf"];
  config.sample_rate  = node_as<double>(rf, "sample_rate", 23.04e6);
  config.nof_channels = node_as<uint32_t>(rf, "num_channels", 1);
  config.uplink_cfo   = node_as<double>(rf, "uplink_cfo", 0);
  config.downlink_cfo = node_as<double>(rf, "downlink_cfo", 0);
  if (rf["padding"]) {
    auto padding         = rf["padding"];
    config.front_padding = node_as<uint32_t>(padding, "front_padding", 0);
    config.back_padding  = node_as<uint32_t>(padding, "back_padding", 0);
  } else {
    config.front_padding = 0;
    config.back_padding  = 0;
  }
  std::cout << "Sample rate: " << config.sample_rate / 1e6 << " MHz" << std::endl;
  std::cout << "Number of channels: " << config.nof_channels << std::endl;

  /* --- channel configs --- */
  if (rf["channels"] && rf["channels"].IsSequence()) {
    for (const auto& chNode : rf["channels"]) {
      if (!chNode["enable"]) {
        continue;
      }
      ChannelConfig ch;
      ch.rx_frequency = node_as<double>(chNode, "rx_frequency", 0);
      ch.tx_frequency = node_as<double>(chNode, "tx_frequency", 0);
      ch.rx_offset    = node_as<double>(chNode, "rx_offset", 0);
      ch.tx_offset    = node_as<double>(chNode, "tx_offset", 0);
      ch.rx_gain      = node_as<double>(chNode, "rx_gain", 40);
      ch.tx_gain      = node_as<double>(chNode, "tx_gain", 80);
      ch.enabled      = node_as<bool>(chNode, "enable", true);
      if (!ch.enabled) {
        continue;
      }
      std::cout << "Channel Config: " << std::endl;
      std::cout << "  RX frequency: " << ch.rx_frequency / 1e6 << " MHz" << std::endl;
      std::cout << "  RX offset: " << ch.rx_offset << " Hz" << std::endl;
      std::cout << "  RX gain: " << ch.rx_gain << " dB" << std::endl;
      std::cout << "  TX frequency: " << ch.tx_frequency / 1e6 << " MHz" << std::endl;
      std::cout << "  TX offset: " << ch.tx_offset << " Hz" << std::endl;
      std::cout << "  TX gain: " << ch.tx_gain << " dB" << std::endl;
      if (ch.rx_frequency == 0 || ch.tx_frequency == 0) {
        std::cerr << "Please provide valid rx and tx frequency for each channel" << std::endl;
        return SRSRAN_ERROR;
      }
      config.channels.push_back(ch);
    }
    config.nof_channels = (uint32_t)config.channels.size();
  } else {
    std::cerr << "Please provide rf channels configuration" << std::endl;
    return SRSRAN_ERROR;
  }

  /* --- worker --- */
  if (!root["workers"]) {
    std::cerr << "Please provide worker configuration" << std::endl;
    return SRSRAN_ERROR;
  }
  auto worker            = root["worker"];
  config.pool_size       = node_as<size_t>(worker, "pool_size", 24);
  config.n_ue_dl_worker  = node_as<uint32_t>(worker, "n_ue_dl_worker", 4);
  config.n_ue_ul_worker  = node_as<uint32_t>(worker, "n_ue_ul_worker", 4);
  config.n_gnb_dl_worker = node_as<uint32_t>(worker, "n_gnb_dl_worker", 4);
  config.n_gnb_ul_worker = node_as<uint32_t>(worker, "n_gnb_ul_worker", 4);

  /* --- ue tracker --- */
  if (!root["uetracker"]) {
    std::cerr << "Please provide ue_tracker configuration" << std::endl;
    return SRSRAN_ERROR;
  }
  auto ue_tracker       = root["uetracker"];
  config.close_timeout  = node_as<uint32_t>(ue_tracker, "close_timeout", 5000);
  config.parse_messages = node_as<bool>(ue_tracker, "parse_messages", true);
  config.num_ues        = node_as<uint32_t>(ue_tracker, "num_ues", 10);
  config.enable_gpu     = node_as<bool>(ue_tracker, "enable_gpu", false);
  printf("Enable GPU acceleration: %s\n", config.enable_gpu ? "true" : "false");

  /* --- injector --- */
  if (!root["downlink_injector"]) {
    std::cerr << "Please provide injector configuration" << std::endl;
    return SRSRAN_ERROR;
  }
  auto injector            = root["downlink_injector"];
  config.delay_n_slots     = node_as<uint32_t>(injector, "delay_n_slots", 4);
  config.duplications      = node_as<uint32_t>(injector, "duplications", 1);
  config.tx_cfo_correction = node_as<float>(injector, "tx_cfo_correction", 0);
  config.tx_advancement    = node_as<int32_t>(injector, "tx_advancement", 0);
  config.ul_advancement    = node_as<uint32_t>(injector, "ul_advancement", 0); // in us
  config.pdsch_mcs         = node_as<uint32_t>(injector, "pdsch_mcs", 3);
  config.pdsch_prbs        = node_as<uint32_t>(injector, "pdsch_prbs", 24);

  /* --- log levels --- */
  if (root["log"]) {
    auto        log              = root["log"];
    std::string log_level        = node_as<std::string>(log, "log_level", "INFO");
    config.log_level             = srslog::str_to_basic_level(log_level);
    std::string syncer_log_level = node_as<std::string>(log, "syncer", "INFO");
    config.syncer_log_level      = srslog::str_to_basic_level(syncer_log_level);
    std::string bc_worker_level  = node_as<std::string>(log, "bc_worker", "INFO");
    config.bc_worker_level       = srslog::str_to_basic_level(bc_worker_level);
    std::string worker_log_level = node_as<std::string>(log, "worker", "INFO");
    config.worker_log_level      = srslog::str_to_basic_level(worker_log_level);
  }

  config.exploit_module = node_as<std::string>(root, "exploit", "");
  printf("Exploit module: %s\n", config.exploit_module.c_str());
  assert(!config.exploit_module.empty());
  return 0;
}