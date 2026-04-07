#include "shadower/comp/workers/broadcast_worker.h"
#include "shadower/utils/arg_parser.h"
#include "shadower/utils/phy_cfg_utils.h"
#include "shadower/utils/utils.h"
#include "srsran/support/srsran_test.h"
#include <unistd.h>

std::string    sample_file;
std::string    mib_config_file;
std::string    sib_config_file;
uint32_t       sib_config_size;
uint32_t       rar_slot_idx = 0;
uint32_t       ncellid      = 1;
uint16_t       rnti         = 0xffff;
ShadowerConfig config       = {};

void parse_args(int argc, char* argv[])
{
  int opt;
  while ((opt = getopt(argc, argv, "sfbcRFiIzZpm")) != -1) {
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
      case 'R': {
        rnti = atoi(argv[optind]);
        printf("Using RNTI: %u\n", rnti);
        break;
      }
      case 'F': {
        sample_file = argv[optind];
        printf("Using sample file: %s\n", sample_file.c_str());
        break;
      }
      case 'i': {
        rar_slot_idx = atoi(argv[optind]);
        printf("Using SIB slot index: %u\n", rar_slot_idx);
        break;
      }
      case 'I': {
        ncellid = atoi(argv[optind]);
        printf("Using cell id: %u\n", ncellid);
        break;
      }
      case 'm': {
        mib_config_file = argv[optind];
        printf("Using mib config file: %s\n", mib_config_file.c_str());
        break;
      }
      case 'z': {
        sib_config_file = argv[optind];
        printf("Using sib config file: %s\n", sib_config_file.c_str());
        break;
      }
      case 'Z': {
        sib_config_size = atoi(argv[optind]);
        printf("Using SIB config size: %u\n", sib_config_size);
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

  /* Read sib1 from file */
  std::vector<uint8_t> sib1_data(sib_config_size);
  if (!read_raw_config(sib_config_file, sib1_data.data(), sib_config_size)) {
    logger.error("Failed to read SIB1 from %s", sib_config_file.c_str());
    return -1;
  }
  /* Decode SIB1 bytes to asn1 structure */
  asn1::rrc_nr_r17::sib1_s sib1 = {};
  if (!parse_to_sib1(sib1_data.data(), sib_config_size, sib1)) {
    logger.error("Failed to parse SIB1");
    return -1;
  }

  /* Update broadcast worker with sib */
  bool            found_new_ue = false;
  BroadCastWorker broadcast_worker(config);
  broadcast_worker.apply_config_from_mib(mib, ncellid);
  broadcast_worker.apply_config_from_sib1(sib1);
  broadcast_worker.set_rnti(rnti, srsran_rnti_type_ra);
  broadcast_worker.on_ue_found =
      [&](uint16_t rnti, std::array<uint8_t, 27UL> rar_grant, uint32_t slot_idx, uint32_t time_advance) {
        logger.info("Found new UE with tc-rnti: %u TA: %u", rnti, time_advance);
        found_new_ue = true;
      };

  /* work on the RAR samples */
  std::shared_ptr<Task>               task    = std::make_shared<Task>();
  std::shared_ptr<std::vector<cf_t> > samples = std::make_shared<std::vector<cf_t> >(sf_len);
  if (!load_samples(sample_file, samples->data(), sf_len)) {
    logger.error("Failed to load samples\n");
    return -1;
  }
  task->dl_buffer[0] = samples;
  task->slot_idx     = rar_slot_idx;
  task->ts           = {};
  bool success       = broadcast_worker.work(task);
  TESTASSERT(success);
  TESTASSERT(found_new_ue);
  return 0;
}