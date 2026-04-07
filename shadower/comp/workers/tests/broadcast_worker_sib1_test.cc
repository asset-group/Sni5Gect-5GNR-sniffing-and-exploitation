#include "shadower/comp/workers/broadcast_worker.h"
#include "shadower/utils/arg_parser.h"
#include "shadower/utils/phy_cfg_utils.h"
#include "shadower/utils/utils.h"
#include "srsran/support/srsran_test.h"
#include <unistd.h>

std::string    sample_file;
std::string    mib_config_file;
uint32_t       sib_slot_idx = 0;
uint32_t       ncellid      = 1;
ShadowerConfig config       = {};

void parse_args(int argc, char* argv[])
{
  int opt;
  while ((opt = getopt(argc, argv, "sfbcFiImp")) != -1) {
    switch (opt) {
      case 's': {
        double srateMHz    = atof(argv[optind]);
        config.sample_rate = srateMHz * 1e6;
        printf("Using sample rate: %f MHz\n", srateMHz);
        break;
      }
      case 'f': {
        double freqMHz = atof(argv[optind]);
        config.dl_freq = freqMHz * 1e6;
        config.ul_freq = config.dl_freq;
        printf("Using center frequency: %f MHz\n", config.dl_freq);
        break;
      }
      case 'b': {
        double ssbFreqMHz = atof(argv[optind]);
        config.ssb_freq   = ssbFreqMHz * 1e6;
        printf("Using SSB Frequency: %f MHz\n", config.ssb_freq);
        break;
      }
      case 'c': {
        config.nof_channels = atoi(argv[optind]);
        printf("Using number of channels: %u\n", config.nof_channels);
        break;
      }
      case 'F': {
        sample_file = argv[optind];
        printf("Using sample file: %s\n", sample_file.c_str());
        break;
      }
      case 'i': {
        sib_slot_idx = atoi(argv[optind]);
        printf("Using SIB slot index: %u\n", sib_slot_idx);
        break;
      }
      case 'I': {
        ncellid = atoi(argv[optind]);
        printf("Using cell id: %u\n", ncellid);
        break;
      }
      case 'm': {
        mib_config_file = argv[optind];
        printf("Using MIB config file: %s\n", mib_config_file.c_str());
        break;
      }
      case 'p': {
        config.nof_prb = atoi(argv[optind]);
        printf("Using number of PRBs: %u\n", config.nof_prb);
        break;
      }
      default:
        fprintf(stderr, "Unknown option or missing argument.\n");
        exit(EXIT_FAILURE);
    }
  }
  config.ssb_pattern = srsran_ssb_pattern_t::SRSRAN_SSB_PATTERN_C;
  config.duplex_mode = srsran_duplex_mode_t::SRSRAN_DUPLEX_MODE_TDD;
  config.scs_ssb     = srsran_subcarrier_spacing_t::srsran_subcarrier_spacing_30kHz;
  config.scs_common  = srsran_subcarrier_spacing_t::srsran_subcarrier_spacing_30kHz;
  if (sample_file.empty()) {
    fprintf(stderr, "Sample file is required.\n");
    exit(EXIT_FAILURE);
  }
}

int main(int argc, char* argv[])
{
  parse_args(argc, argv);
  /* initialize logger */
  config.bc_worker_level       = srslog::basic_levels::debug;
  srslog::basic_logger& logger = srslog_init(&config);
  logger.set_level(srslog::basic_levels::debug);
  uint32_t sf_len = config.sample_rate * SF_DURATION;

  /* Read mib from file */
  srsran_mib_nr_t mib = {};
  if (!read_raw_config(mib_config_file, (uint8_t*)&mib, sizeof(srsran_mib_nr_t))) {
    logger.error("Failed to read MIB from %s", mib_config_file.c_str());
    return -1;
  }

  /* Show MIB information */
  std::array<char, 512> mib_info_str = {};
  srsran_pbch_msg_nr_mib_info(&mib, mib_info_str.data(), mib_info_str.size());
  logger.info("Applying MIB config: %s", mib_info_str.data());

  /* Initialize broadcast worker */
  bool            found_sib1 = false;
  BroadCastWorker broadcast_worker(config);
  broadcast_worker.apply_config_from_mib(mib, ncellid);
  broadcast_worker.on_sib1_found = [&](asn1::rrc_nr_r17::sib1_s& sib1) {
    broadcast_worker.apply_config_from_sib1(sib1);
    found_sib1 = true;
  };

  /* work on the SIB1 samples */
  std::shared_ptr<Task>               task    = std::make_shared<Task>();
  std::shared_ptr<std::vector<cf_t> > samples = std::make_shared<std::vector<cf_t> >(sf_len);
  if (!load_samples(sample_file, samples->data(), sf_len)) {
    logger.error("Failed to load samples\n");
    return -1;
  }
  task->dl_buffer[0] = samples;
  task->slot_idx     = sib_slot_idx;
  task->ts           = {};
  bool success       = broadcast_worker.work(task);
  TESTASSERT(success);
  TESTASSERT(found_sib1);
  return 0;
}