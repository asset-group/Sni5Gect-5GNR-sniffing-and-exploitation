#include "shadower/utils/utils.h"
#include "srsran/mac/mac_sch_pdu_nr.h"
#include "srsran/phy/phch/pbch_msg_nr.h"
#include "srsran/phy/ue/ue_dl_nr.h"
#include "test_variables.h"

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
  std::string           last_sample_file;
  uint32_t              rach_msg2_slot_idx;
  uint32_t              rach_msg3_slot_idx;
  int                   ul_sample_offset;
  uint32_t              half = 1;
  switch (test_number) {
    case 0:
      sample_file        = "shadower/test/data/srsran-n78-20MHz/rach_msg3.fc32";
      rach_msg2_slot_idx = 10;
      rach_msg3_slot_idx = 16;
      ul_sample_offset   = 468;
      half               = 1;
      break;
    case 1:
      sample_file        = "shadower/test/data/srsran-n78-40MHz/rach_msg3.fc32";
      rach_msg2_slot_idx = 10;
      rach_msg3_slot_idx = 16;
      ul_sample_offset   = 768;
      half               = 1;
      break;
    case 2:
      sample_file        = "shadower/test/data/effnet/rach_msg3.fc32";
      rach_msg2_slot_idx = 5;
      rach_msg3_slot_idx = 8;
      ul_sample_offset   = 468;
      half               = 1;
      break;
    case 4:
      sample_file        = "shadower/test/data/srsran-n3-20MHz/rach_msg3.fc32";
      last_sample_file   = "shadower/test/data/srsran-n3-20MHz/rach_msg3_last.fc32";
      rach_msg2_slot_idx = 17;
      rach_msg3_slot_idx = 23;
      ul_sample_offset   = 480;
      half               = 0;
      break;
    default:
      fprintf(stderr, "Unknown test number: %d\n", test_number);
      exit(EXIT_FAILURE);
  }

  /* initialize phy cfg */
  srsran::phy_cfg_nr_t phy_cfg = {};
  init_phy_cfg(phy_cfg, config);

  /* init phy state */
  srsue::nr::state phy_state = {};
  init_phy_state(phy_state, config.nof_prb);

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

  /* GNB UL initialize with configuration from phy_cfg */
  srsran_gnb_ul_t gnb_ul        = {};
  cf_t*           gnb_ul_buffer = srsran_vec_cf_malloc(args.sf_len);
  if (!init_gnb_ul(gnb_ul, gnb_ul_buffer, phy_cfg)) {
    logger.error("Failed to init GNB UL");
    return -1;
  }

  /* load raw grant data carried in rach msg2 */
  std::array<uint8_t, SRSRAN_RAR_UL_GRANT_NBITS> ul_grant_raw{};
  if (!read_raw_config(args.rar_ul_grant_file, ul_grant_raw.data(), SRSRAN_RAR_UL_GRANT_NBITS)) {
    logger.error("Failed to read RAR UL grant from %s", args.rar_ul_grant_file.c_str());
    return -1;
  }

  /* load test samples */
  std::vector<cf_t> samples(args.sf_len);
  if (!load_samples(sample_file, samples.data(), args.sf_len)) {
    logger.error("Failed to load data from %s", sample_file.c_str());
    return -1;
  }

  std::vector<cf_t> last_samples(args.sf_len);
  if (!last_samples.empty() && last_sample_file != "") {
    if (!load_samples(last_sample_file, last_samples.data(), args.sf_len)) {
      logger.error("Failed to load data from %s", last_sample_file.c_str());
      return -1;
    }
  }

  srsran_softbuffer_rx_t softbuffer_rx = {};
  if (srsran_softbuffer_rx_init_guru(&softbuffer_rx, SRSRAN_SCH_NR_MAX_NOF_CB_LDPC, SRSRAN_LDPC_MAX_LEN_ENCODED_CB) !=
      0) {
    logger.error("Couldn't allocate and/or initialize softbuffer");
    return -1;
  }

  /* Add rar grant to phy_state */
  uint32_t grant_k = 0;
  if (!set_rar_grant(
          args.c_rnti, srsran_rnti_type_c, rach_msg2_slot_idx, ul_grant_raw, phy_cfg, phy_state, &grant_k, logger)) {
    logger.error("Failed to set RAR grant");
    return -1;
  }

  /* copy samples to gnb_ul processing buffer */
  if (half < 1 && last_sample_file.empty()) {
    logger.error("Last subframe is required");
    return -1;
  }
  if (half > 0) {
    srsran_vec_cf_copy(gnb_ul_buffer, samples.data() + half * args.slot_len - ul_sample_offset, args.slot_len);
  } else {
    srsran_vec_cf_copy(gnb_ul_buffer, last_samples.data() + args.slot_len - ul_sample_offset, ul_sample_offset);
    srsran_vec_cf_copy(gnb_ul_buffer + ul_sample_offset, samples.data(), args.slot_len - ul_sample_offset);
  }
  srsran_slot_cfg_t slot_cfg = {.idx = rach_msg3_slot_idx + half};

  /* get uplink grant */
  uint32_t            pid       = 0;
  srsran_sch_cfg_nr_t pusch_cfg = {};
  if (!phy_state.get_ul_pending_grant(slot_cfg.idx, pusch_cfg, pid)) {
    logger.error("No uplink grant available");
    return -1;
  }

  /* run gnb_ul estimate fft */
  if (srsran_gnb_ul_fft(&gnb_ul)) {
    logger.error("Error running srsran_gnb_ul_fft");
    return -1;
  }

  char filename[64];
  sprintf(filename, "ofdm_rach_msg3_fft%u", args.nof_sc);
  write_record_to_file(gnb_ul.sf_symbols[0], args.nof_re, filename);

  /* Apply the cfo to the signal with magic number */
  srsran_vec_apply_cfo(gnb_ul.sf_symbols[0], config.uplink_cfo, gnb_ul.sf_symbols[0], args.nof_re);

  /* Initialize the buffer for output*/
  srsran::unique_byte_buffer_t data = srsran::make_byte_buffer();
  if (data == nullptr) {
    logger.error("Error creating byte buffer");
    return -1;
  }
  data->N_bytes = pusch_cfg.grant.tb[0].tbs / 8U;

  /* Initialize pusch result*/
  srsran_pusch_res_nr_t pusch_res = {};
  pusch_res.tb[0].payload         = data->msg;
  srsran_softbuffer_rx_reset(&softbuffer_rx);

  /* Decode PUSCH */
  if (!gnb_ul_pusch_decode(gnb_ul, pusch_cfg, slot_cfg, pusch_res, softbuffer_rx, logger)) {
    logger.error("Error running gnb_ul_pusch_decode");
    return -1;
  }

  /* if the message is not decoded correctly, then return */
  if (!pusch_res.tb[0].crc) {
    return -1;
  } else {
    logger.info("PUSCH CRC passed Delay: %u CFO: %f", ul_sample_offset, config.uplink_cfo);
  }
  return 0;
}