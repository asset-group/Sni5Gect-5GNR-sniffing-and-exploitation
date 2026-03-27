#include "shadower/utils/utils.h"
#include "srsran/common/mac_pcap.h"
#include <getopt.h>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <vector>

int main(int argc, char* argv[])
{
  if (argc < 2) {
    std::cerr << "Usage: " << argv[0] << " <hex_data> (space-separated, e.g., '80 48 00 38 16 53')" << std::endl;
    return 1;
  }

  bool is_dl = true;
  int  opt;
  while ((opt = getopt(argc, argv, "u")) != -1) {
    switch (opt) {
      case 'u':
        is_dl = false;
        break;
      default:
        std::cerr << "Usage: " << argv[0] << " <hex_data> (space-separated, e.g., '80 48 00 38 16 53')" << std::endl;
        return 1;
    }
  }
  ShadowerConfig config;
  config.log_level             = srslog::basic_levels::info;
  srslog::basic_logger& logger = srslog_init(&config);

  // Parse hex data from command line
  uint32_t    size = 0;
  uint8_t     data[4096];
  std::string hex_str = argv[1];
  hex_to_bytes(hex_str, data, &size);

  srsran::mac_pcap* writer = new srsran::mac_pcap();
  if (writer->open("logs/debug.pcap")) {
    printf("Failed to open pcap file\n");
  }
  writer->write_dl_crnti_nr(data, size, 0x10b, 4, 10);
  // writer->write_dl_ra_rnti_nr(data, size, 0x10b, 4, 10);
  writer->close();
}