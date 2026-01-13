#include "shadower/utils/ssb_utils.h"
#include "shadower/utils/utils.h"
#include "srsran/phy/phch/pbch_msg_nr.h"
#include "srsran/phy/sync/ssb.h"
#include "srsran/srslog/srslog.h"
#include "srsran/support/srsran_test.h"
#include "test_variables.h"
#include <fstream>
#if ENABLE_CUDA
#include "shadower/comp/ssb/ssb_cuda.cuh"
#endif // ENABLE_CUDA

int main(int argc, char* argv[])
{
  int test_number = 0;
  if (argc > 1) {
    test_number = atoi(argv[1]);
  }
  test_args_t     args   = init_test_args(test_number);
  ShadowerConfig& config = args.config;
  /* initialize logger */
  srslog::basic_logger& logger = srslog_init(&config);
  std::string           sample_file;
  switch (test_number) {
    case 0:
      sample_file = "shadower/test/data/srsran-n78-20MHz/sib.fc32";
      break;
    case 1:
      sample_file = "shadower/test/data/srsran-n78-40MHz/sib.fc32";
      break;
    case 2:
      sample_file = "shadower/test/data/effnet/sib.fc32";
      break;
    case 3:
      sample_file = "shadower/test/data/singtel-n78-100MHz/ssb.fc32";
      break;
    case 4:
      sample_file = "shadower/test/data/srsran-n3-20MHz/ssb.fc32";
      break;
    case 5:
      sample_file = "shadower/test/data/singtel-n1-20MHz/ssb.fc32";
      break;
    case 6:
      sample_file = "shadower/test/data/srsran-n5-10MHz/ssb.fc32";
      break;
    default:
      fprintf(stderr, "Unknown test number: %d\n", test_number);
      exit(EXIT_FAILURE);
  }

  /* initialize ssb */
  srsran_ssb_t ssb = {};
  if (!init_ssb(ssb,
                config.sample_rate,
                config.dl_freq,
                config.ssb_freq,
                config.scs_ssb,
                config.ssb_pattern,
                config.duplex_mode)) {
    logger.error("Failed to initialize SSB");
    return -1;
  }

  /* load samples from file */
  std::vector<cf_t> samples(args.sf_len);
  if (!load_samples(sample_file, samples.data(), args.sf_len)) {
    logger.error("Failed to load data from %s", sample_file.c_str());
    return -1;
  }

  /* search for SSB */
  srsran_ssb_search_res_t res = {};
  if (srsran_ssb_search(&ssb, samples.data(), args.sf_len, &res) < SRSRAN_SUCCESS) {
    logger.error("Error running srsran_ssb_search");
    return -1;
  }
  if (res.measurements.snr_dB < -10.0f) {
    logger.error("SNR is too low: %f dB", res.measurements.snr_dB);
    return -1;
  }
  if (!res.pbch_msg.crc) {
    logger.error("Failed to decode PBCH message CRC error");
    return -1;
  }

  /* decode MIB */
  srsran_mib_nr_t mib = {};
  if (srsran_pbch_msg_nr_mib_unpack(&res.pbch_msg, &mib) < SRSRAN_SUCCESS) {
    logger.error("Error running srsran_pbch_msg_nr_mib_unpack");
    return -1;
  }

  /* get SSB index */
  uint32_t sf_idx   = srsran_ssb_candidate_sf_idx(&ssb, res.pbch_msg.ssb_idx, res.pbch_msg.hrf);
  uint32_t slot_idx = mib.sfn * 10 * args.slot_per_sf + sf_idx;
  logger.info("SF index: %u Slot index: %u", sf_idx, slot_idx);

  /* write MIB to file */
  std::array<char, 512> mib_info_str = {};
  srsran_pbch_msg_nr_mib_info(&mib, mib_info_str.data(), mib_info_str.size());
  std::ofstream mib_raw{args.mib_config_raw, std::ios::binary};
  mib_raw.write(reinterpret_cast<const char*>(&mib), sizeof(mib));

  logger.info("Delay: %f us", res.measurements.delay_us);
  logger.info("Found cell: %s", mib_info_str.data());
  logger.info("Cell id: %u", res.N_id);
  logger.info("Offset: %u", res.t_offset);
  logger.info("CFO: %f", res.measurements.cfo_hz);
  TESTASSERT(res.pbch_msg.crc);
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  return 0;
}
