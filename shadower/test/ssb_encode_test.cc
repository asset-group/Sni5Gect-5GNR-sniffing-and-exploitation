extern "C" {
#include "srsran/phy/sync/sss_nr.h"
}
#include "shadower/test/test_variables.h"
#include "shadower/utils/utils.h"
#include "srsran/common/phy_cfg_nr.h"
#include "srsran/phy/sync/ssb.h"

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

  /* initialize phy_cfg */
  srsran::phy_cfg_nr_t phy_cfg = {};
  init_phy_cfg(phy_cfg, config);

  /* load mib configuration and update phy_cfg */
  if (!configure_phy_cfg_from_mib(phy_cfg, args.mib_config_raw, args.ncellid)) {
    logger.error("Failed to configure phy cfg from mib");
    return -1;
  }

  /* load sib1 configuration and apply to phy_cfg */
  if (!configure_phy_cfg_from_sib1(phy_cfg, args.sib_config_raw, args.sib_size)) {
    logger.error("Failed to configure phy cfg from sib1");
    return -1;
  }

  /* load rrc setup configuration and apply to phy_cfg */
  if (!configure_phy_cfg_from_rrc_setup(phy_cfg, args.rrc_setup_raw, args.rrc_setup_size, logger)) {
    logger.error("Failed to configure phy cfg from rrc setup");
    return -1;
  }

  /* initialize SSB */
  srsran_ssb_t ssb = {};

  srsran_ssb_args_t ssb_args = {};
  ssb_args.max_srate_hz      = config.sample_rate;
  ssb_args.min_scs           = config.scs_ssb;
  ssb_args.enable_search     = true;
  ssb_args.enable_measure    = true;
  ssb_args.enable_decode     = true;
  ssb_args.enable_encode     = true;
  if (srsran_ssb_init(&ssb, &ssb_args) != 0) {
    logger.error("Failed to initialize ssb");
    return -1;
  }

  /* Set SSB config */
  srsran_ssb_cfg_t ssb_cfg = {};
  ssb_cfg.srate_hz         = config.sample_rate;
  ssb_cfg.center_freq_hz   = config.dl_freq;
  ssb_cfg.ssb_freq_hz      = config.ssb_freq;
  ssb_cfg.scs              = config.scs_ssb;
  ssb_cfg.pattern          = config.ssb_pattern;
  ssb_cfg.duplex_mode      = config.duplex_mode;
  ssb_cfg.periodicity_ms   = 10;
  if (srsran_ssb_set_cfg(&ssb, &ssb_cfg) < SRSRAN_SUCCESS) {
    logger.error("Failed to set ssb config");
    return -1;
  }

  /* GNB DL init with configuration from phy_cfg */
  srsran_gnb_dl_t gnb_dl        = {};
  cf_t*           gnb_dl_buffer = srsran_vec_cf_malloc(args.slot_len);
  if (!init_gnb_dl(gnb_dl, gnb_dl_buffer, phy_cfg, config.sample_rate)) {
    logger.error("Failed to init GNB DL");
    return -1;
  }

  /* Add ssb config to gnb_dl */
  srsran_gnb_dl_set_ssb_config(&gnb_dl, &ssb_cfg);
  srsran_mib_nr_t mib        = {};
  mib.sfn                    = 100;
  mib.ssb_idx                = 0;
  mib.hrf                    = false;
  mib.scs_common             = config.scs_ssb;
  mib.ssb_offset             = 14;
  mib.dmrs_typeA_pos         = srsran_dmrs_sch_typeA_pos_2;
  mib.coreset0_idx           = 2;
  mib.ss0_idx                = 2;
  mib.cell_barred            = false;
  mib.intra_freq_reselection = false;
  mib.spare                  = 0;

  srsran_pbch_msg_nr_t pbch_msg = {};
  if (srsran_pbch_msg_nr_mib_pack(&mib, &pbch_msg) < SRSRAN_SUCCESS) {
    logger.error("Failed to pack MIB into PBCH message");
    return -1;
  }

  if (pbch_msg.crc != SRSRAN_SUCCESS) {
    logger.error("Failed to pack MIB into PBCH message, CRC error");
    return -1;
  }

  if (srsran_gnb_dl_add_ssb(&gnb_dl, &pbch_msg, mib.sfn) < SRSRAN_SUCCESS) {
    logger.error("Failed to add SSB");
    return -1;
  }

  char filename[64];
  sprintf(filename, "ssb_generated.fc32");
  write_record_to_file(gnb_dl_buffer, args.slot_len, filename);

  /* Decode the generated SSB */
  srsran_ssb_search_res_t res = {};
  if (srsran_ssb_search(&ssb, gnb_dl_buffer, args.slot_len, &res) < SRSRAN_SUCCESS) {
    logger.error("Failed to search SSB");
    return -1;
  }
  if (res.measurements.snr_dB < -10.0f || res.N_id != args.ncellid) {
    logger.error("SSB SNR too low or N_id mismatch: SNR: %.2f dB, N_id: %u", res.measurements.snr_dB, res.N_id);
    return -1;
  }

  if (!res.pbch_msg.crc) {
    logger.error("Failed to decode PBCH message, CRC error");
    return -1;
  }
  logger.info("SSB generation successful, N_id: %u, SNR: %.2f dB", res.N_id, res.measurements.snr_dB);

  srsran_mib_nr_t mib_decoded = {};
  if (srsran_pbch_msg_nr_mib_unpack(&res.pbch_msg, &mib_decoded) < SRSRAN_SUCCESS) {
    logger.error("Failed to unpack PBCH message");
    return -1;
  }

  std::array<char, 512> mib_info_str = {};
  srsran_pbch_msg_nr_mib_info(&mib_decoded, mib_info_str.data(), mib_info_str.size());
  logger.info("MIB info: %s", mib_info_str.data());

  std::this_thread::sleep_for(std::chrono::milliseconds(100));
}