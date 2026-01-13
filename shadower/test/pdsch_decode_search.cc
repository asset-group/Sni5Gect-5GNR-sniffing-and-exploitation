#include "shadower/utils/utils.h"
#include "srsran/asn1/rrc_nr.h"
#include "srsran/mac/mac_sch_pdu_nr.h"
#include "srsran/phy/phch/pbch_msg_nr.h"
#include "srsran/phy/ue/ue_dl_nr.h"
#include "test_variables.h"
#include <unistd.h>

std::string sample_file;
std::string last_sample_file;
std::string next_sample_file;
uint32_t    slot_number;
uint32_t    half  = 0;
int         begin = -100;
int         end   = 100;

void parse_args(int argc, char* argv[])
{
  int opt;
  while ((opt = getopt(argc, argv, "f:s:h:l:n:b:e:")) != -1) {
    switch (opt) {
      case 'f':
        sample_file = optarg;
        break;
      case 's':
        slot_number = atoi(optarg);
        break;
      case 'h':
        half = atoi(optarg);
        break;
      case 'l':
        last_sample_file = optarg;
        break;
      case 'n':
        next_sample_file = optarg;
        break;
      case 'b':
        begin = atoi(optarg);
        break;
      case 'e':
        end = atoi(optarg);
        break;
      default:
        fprintf(stderr, "Unknown option: %c\n", opt);
        exit(EXIT_FAILURE);
    }
  }
}

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
  switch (test_number) {
    case 0:
      sample_file = "shadower/test/data/srsran-n78-20MHz/pdsch_3440.fc32";
      slot_number = 0;
      half        = 0;
      break;
    case 1:
      sample_file = "shadower/test/data/srsran-n78-40MHz/pdsch_13640.fc32";
      slot_number = 0;
      half        = 0;
      break;
    case 2:
      sample_file = "/root/overshadow/effnet/sf_152_11864.fc32";
      slot_number = 4;
      half        = 1;
      break;
    case 4:
      sample_file = "shadower/test/data/srsran-n3-20MHz/pdsch_3357.fc32";
      slot_number = 17;
      half        = 0;
      break;
    default:
      fprintf(stderr, "Unknown test number: %d\n", test_number);
      exit(EXIT_FAILURE);
  }
  /* parse command line arguments */
  parse_args(argc, argv);

  logger.info("Sample file: %s", sample_file.c_str());
  logger.info("Slot number: %u", slot_number);
  logger.info("Half: %u", half);

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

  /* load rrc setup configuration and apply to phy_cfg */
  if (!configure_phy_cfg_from_rrc_setup(phy_cfg, args.rrc_setup_raw, args.rrc_setup_size, logger)) {
    logger.error("Failed to configure phy cfg from rrc setup");
    return -1;
  }

  /* UE DL init with configuration from phy_cfg */
  srsran_ue_dl_nr_t ue_dl  = {};
  cf_t*             buffer = srsran_vec_cf_malloc(args.sf_len);
  if (!init_ue_dl(ue_dl, buffer, phy_cfg)) {
    logger.error("Failed to init UE DL");
    return -1;
  }

  /* load test samples */
  std::vector<cf_t> samples(args.sf_len);
  if (!load_samples(sample_file, samples.data(), args.sf_len)) {
    logger.error("Failed to load data from %s", sample_file.c_str());
    return -1;
  }

  std::vector<cf_t> last_samples(args.sf_len);
  if (!last_sample_file.empty() && !load_samples(last_sample_file, last_samples.data(), args.sf_len)) {
    logger.error("Failed to load data from %s", last_sample_file.c_str());
    return -1;
  }

  std::vector<cf_t> next_samples(args.sf_len);
  if (!next_sample_file.empty() && !load_samples(next_sample_file, next_samples.data(), args.sf_len)) {
    logger.error("Failed to load data from %s", next_sample_file.c_str());
    return -1;
  }

  for (int offset = begin; offset < end; offset++) {
    if (half == 0 && offset < 0) {
      /* Copy the last section of the samples to the buffer */
      if (!last_sample_file.empty()) {
        srsran_vec_cf_copy(buffer, last_samples.data() + args.sf_len + offset, -offset);
      } else {
        srsran_vec_cf_zero(buffer, -offset); // Zero fill if no last sample file
      }
      /* Copy the current section to the offset */
      srsran_vec_cf_copy(buffer - offset, samples.data(), args.slot_len + offset);
    } else if (half > 0 && offset > 0) {
      srsran_vec_cf_copy(buffer, samples.data() + args.slot_len * half + offset, args.slot_len - offset);
      if (!next_sample_file.empty()) {
        srsran_vec_cf_copy(buffer + args.slot_len * half - offset, next_samples.data(), offset);
      } else {
        srsran_vec_cf_zero(buffer + args.slot_len * half - offset, offset); // Zero fill if no next sample file
      }
    } else {
      srsran_vec_cf_copy(buffer, samples.data() + args.slot_len * half + offset, args.slot_len);
    }

    /* Initialize slot cfg */
    srsran_slot_cfg_t slot_cfg = {.idx = slot_number + half};
    /* run ue_dl estimate fft */
    srsran_ue_dl_nr_estimate_fft(&ue_dl, &slot_cfg);

    /* Write OFDM symbols to file for debug purpose */
    char filename[64];
    sprintf(filename, "ofdm_pdsch_fft%u", args.nof_sc);
    write_record_to_file(ue_dl.sf_symbols[0], args.nof_re, filename);

    std::array<srsran_dci_dl_nr_t, SRSRAN_SEARCH_SPACE_MAX_NOF_CANDIDATES_NR> dci_dl = {};
    std::array<srsran_dci_ul_nr_t, SRSRAN_SEARCH_SPACE_MAX_NOF_CANDIDATES_NR> dci_ul = {};
    /* search for dci */
    ue_dl_dci_search(ue_dl, phy_cfg, slot_cfg, args.c_rnti, srsran_rnti_type_c, phy_state, logger, 0, dci_dl, dci_ul);

    /* get grant from dci search */
    uint32_t                   pid          = 0;
    srsran_sch_cfg_nr_t        pdsch_cfg    = {};
    srsran_harq_ack_resource_t ack_resource = {};
    if (!phy_state.get_dl_pending_grant(slot_cfg.idx, pdsch_cfg, ack_resource, pid)) {
      logger.error("Offset: %d Failed to get grant from dci search", offset);
      continue;
    }
    /* Initialize the buffer for output*/
    srsran::unique_byte_buffer_t data = srsran::make_byte_buffer();
    if (data == nullptr) {
      logger.error("Error creating byte buffer");
      continue;
    }
    data->N_bytes = pdsch_cfg.grant.tb[0].tbs / 8U;

    /* Initialize pdsch result*/
    srsran_pdsch_res_nr_t pdsch_res      = {};
    pdsch_res.tb[0].payload              = data->msg;
    srsran_softbuffer_rx_t softbuffer_rx = {};
    if (srsran_softbuffer_rx_init_guru(&softbuffer_rx, SRSRAN_SCH_NR_MAX_NOF_CB_LDPC, SRSRAN_LDPC_MAX_LEN_ENCODED_CB) !=
        0) {
      logger.error("Couldn't allocate and/or initialize softbuffer");
      continue;
    }

    /* Decode PDSCH */
    if (!ue_dl_pdsch_decode(ue_dl, pdsch_cfg, slot_cfg, pdsch_res, softbuffer_rx, logger, 0)) {
      continue;
    }
    /* if the message is not decoded correctly, then return */
    if (!pdsch_res.tb[0].crc) {
      logger.debug("Offset: %d Error PDSCH got wrong CRC", offset);
      continue;
    }
    logger.info("Offset: %d PDSCH decoded successfully", offset);
  }
  return 0;
}