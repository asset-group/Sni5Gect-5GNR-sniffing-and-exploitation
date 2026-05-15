#include "shadower/comp/sync/syncer.h"
#include "shadower/comp/workers/broadcast_worker.h"
#include "shadower/source/source.h"
#include "shadower/utils/constants.h"
#include "shadower/utils/phy_cfg_utils.h"
#include "shadower/utils/safe_queue.h"
#include "shadower/utils/utils.h"
#include "srsran/asn1/rrc_nr/bcch_dl_sch_msg.h"
#include "srsran/common/band_helper.h"
#include "srsran/phy/common/phy_common_nr.h"
#include "srsran/phy/sync/ssb.h"
#include <atomic>
#include <csignal>
#include <functional>
#include <getopt.h>
#include <memory>
#include <thread>

static std::atomic<bool> stop_flag{false};       /* set by SIGINT — exits the outer scan loop */
static std::atomic<bool> cell_done{false};       /* set when SIB1 decoded or source error for current cell */
static std::atomic<bool> cell_found_flag{false}; /* gates task delivery to the decoder */

static ShadowerConfig              config        = {};
static uint32_t                    sf_len        = 0;
static double                      start_freq    = 0;
static double                      stop_freq     = 0;
static double                      sample_rate   = 23.04e6;
static uint32_t                    band          = 78;
static uint32_t                    rounds        = 100;
static uint32_t                    rx_gain       = 40;
static std::string                 source_params = "type=b200";
static srsran_subcarrier_spacing_t scs           = srsran_subcarrier_spacing_30kHz;

static SafeQueue<Task>                  task_queue;
static std::shared_ptr<BroadCastWorker> bc_worker;

static void sigint_handler(int)
{
  stop_flag.store(true);
}

static void push_new_task(std::shared_ptr<Task>& task)
{
  if (!cell_found_flag.load()) {
    return;
  }
  task_queue.push(task);
}

static void usage(const char* prog)
{
  printf("Usage: %s [options]\n", prog);
  printf("  -f <MHz>  Start frequency in MHz (required)\n");
  printf("  -e <MHz>  Stop frequency in MHz (required)\n");
  printf("  -s <MHz>  Sample rate in MHz (default: 23.04)\n");
  printf("  -b <n>    NR band number (default: 78)\n");
  printf("  -S <kHz>  SSB subcarrier spacing: 15, 30, 60, 120 (default: 30)\n");
  printf("  -d <str>  UHD device args (default: type=b200)\n");
  printf("  -g <dB>   RX gain in dB (default: 40)\n");
  printf("  -r <n>    Rounds per SSB frequency during scan (default: 100)\n");
}

static void parse_args(int argc, char* argv[])
{
  int opt;
  while ((opt = getopt(argc, argv, "f:e:s:b:S:d:g:r:")) != -1) {
    switch (opt) {
      case 'f':
        start_freq = atof(optarg) * 1e6;
        break;
      case 'e':
        stop_freq = atof(optarg) * 1e6;
        break;
      case 's':
        sample_rate = atof(optarg) * 1e6;
        break;
      case 'b':
        band = atoi(optarg);
        break;
      case 'S':
        scs = srsran_subcarrier_spacing_from_str(optarg);
        break;
      case 'd':
        source_params = optarg;
        break;
      case 'g':
        rx_gain = atoi(optarg);
        break;
      case 'r':
        rounds = atoi(optarg);
        break;
      default:
        usage(argv[0]);
        exit(EXIT_FAILURE);
    }
  }
  if (start_freq == 0 || stop_freq == 0) {
    fprintf(stderr, "Error: -f and -e are required\n");
    usage(argv[0]);
    exit(EXIT_FAILURE);
  }
  sf_len = (uint32_t)(sample_rate * SF_DURATION);
}

/* Search for an SSB block at ssb_freq. Returns true if found and fills mib_out / N_id_out. */
static bool scan_ssb(Source*               source,
                     double                ssb_freq,
                     srslog::basic_logger& logger,
                     srsran_ssb_pattern_t  ssb_pattern,
                     srsran_duplex_mode_t  duplex_mode,
                     uint32_t              n_rounds,
                     srsran_mib_nr_t&      mib_out,
                     uint32_t&             N_id_out)
{
  srsran_ssb_t ssb = {};
  if (!init_ssb(ssb, sample_rate, ssb_freq, ssb_freq, scs, ssb_pattern, duplex_mode)) {
    logger.error("Error initializing SSB at %.3f MHz", ssb_freq / 1e6);
    srsran_ssb_free(&ssb);
    return false;
  }

  source->set_rx_freq(ssb_freq);

  cf_t* buffer                         = srsran_vec_cf_malloc(sf_len);
  cf_t* rx_buffer[SRSRAN_MAX_CHANNELS] = {};
  rx_buffer[0]                         = buffer;
  bool found                           = false;

  for (uint32_t i = 0; i < n_rounds && !found && !stop_flag.load(); i++) {
    srsran_timestamp_t ts = {};
    /* Flush a small burst to clear stale SDR buffers, then receive a full subframe */
    source->recv(rx_buffer, (uint32_t)(sf_len * SF_DURATION), &ts);
    source->recv(rx_buffer, sf_len, &ts);

    srsran_ssb_search_res_t res = {};
    if (srsran_ssb_search(&ssb, buffer, sf_len, &res) < SRSRAN_SUCCESS) {
      logger.error("Error srsran_ssb_search");
      break;
    }
    if (res.measurements.snr_dB < -10.0f || !res.pbch_msg.crc) {
      continue;
    }
    srsran_mib_nr_t mib = {};
    if (srsran_pbch_msg_nr_mib_unpack(&res.pbch_msg, &mib) < SRSRAN_SUCCESS) {
      logger.error("Error srsran_pbch_msg_nr_mib_unpack");
      continue;
    }
    std::array<char, 512> mib_info = {};
    srsran_pbch_msg_nr_mib_info(&mib, mib_info.data(), (uint32_t)mib_info.size());
    logger.info("Found SSB at %.3f MHz: cell_id=%u %s SNR=%.1f dB CFO=%.1f Hz",
                ssb_freq / 1e6,
                res.N_id,
                mib_info.data(),
                res.measurements.snr_dB,
                res.measurements.cfo_hz);
    mib_out  = mib;
    N_id_out = res.N_id;
    found    = true;
  }

  srsran_ssb_free(&ssb);
  free(buffer);
  return found;
}

/* Look up the CORESET 0 table entry and compute the center frequency and PRB count. */
static bool compute_coreset0(const srsran_mib_nr_t&      mib,
                             double                      ssb_freq,
                             srsran_subcarrier_spacing_t scs_ssb,
                             srslog::basic_logger&       logger,
                             double&                     center_freq_out,
                             uint32_t&                   nof_prb_out)
{
  /* SSB occupies 20 RBs; its bottom edge is half the SSB bandwidth below the SSB center */
  double ssb_bottom = ssb_freq - (20.0 * 12 * 1000 * (15 << (uint32_t)scs_ssb)) / 2.0;
  /* CRB 0 bottom is ssb_offset subcarriers below the SSB bottom edge */
  double crb_bottom = ssb_bottom - (mib.ssb_offset * 1000.0 * (15 << (uint32_t)mib.scs_common));

  const coreset_zero_entry_t* entry = nullptr;
  if (scs_ssb == srsran_subcarrier_spacing_15kHz && mib.scs_common == srsran_subcarrier_spacing_15kHz) {
    entry = &coreset_zero_15_15[mib.coreset0_idx];
  } else if (scs_ssb == srsran_subcarrier_spacing_30kHz && mib.scs_common == srsran_subcarrier_spacing_30kHz) {
    entry = &coreset_zero_30_30[mib.coreset0_idx];
  } else if (scs_ssb == srsran_subcarrier_spacing_15kHz && mib.scs_common == srsran_subcarrier_spacing_30kHz) {
    entry = &coreset_zero_15_30[mib.coreset0_idx];
  } else if (scs_ssb == srsran_subcarrier_spacing_30kHz && mib.scs_common == srsran_subcarrier_spacing_15kHz) {
    entry = &coreset_zero_30_15[mib.coreset0_idx];
  }

  if (entry == nullptr || entry->nof_prb == 0) {
    logger.error("Unsupported SCS pair or invalid coreset0_idx %u", mib.coreset0_idx);
    return false;
  }

  double sc_hz           = 1000.0 * (15 << (uint32_t)mib.scs_common);
  double coreset0_bottom = crb_bottom - (entry->offset_rb * 12.0 * sc_hz);
  double coreset0_bw     = entry->nof_prb * 12.0 * sc_hz;

  center_freq_out = coreset0_bottom + coreset0_bw / 2.0;
  nof_prb_out     = entry->nof_prb;

  logger.info("SSB bottom edge      : %.3f MHz", ssb_bottom / 1e6);
  logger.info("CRB 0 bottom edge    : %.3f MHz", crb_bottom / 1e6);
  logger.info("CORESET 0 bottom edge: %.3f MHz", coreset0_bottom / 1e6);
  logger.info("CORESET 0 top edge   : %.3f MHz", (coreset0_bottom + coreset0_bw) / 1e6);
  logger.info("CORESET 0 center     : %.3f MHz", center_freq_out / 1e6);
  logger.info("CORESET 0 PRBs       : %u", nof_prb_out);
  return true;
}

/* Decoder thread: pops tasks and dispatches to BroadcastWorker. nullptr sentinel stops it. */
static void decoder_worker()
{
  srslog::basic_logger& logger = srslog::fetch_basic_logger("scanner");
  logger.info("Decoder worker started");
  while (true) {
    std::shared_ptr<Task> task = task_queue.retrieve();
    if (!task) {
      break;
    }
    bc_worker->work(task);
  }
  logger.info("Decoder worker stopped");
}

int main(int argc, char* argv[])
{
  parse_args(argc, argv);
  std::signal(SIGINT, sigint_handler);

  config.log_level             = srslog::basic_levels::info;
  config.syncer_log_level      = srslog::basic_levels::info;
  config.bc_worker_level       = srslog::basic_levels::info;
  config.worker_log_level      = srslog::basic_levels::info;
  srslog::basic_logger& logger = srslog_init(&config);

  /* Create the SDR source — reused across all cells in the scan */
  ShadowerConfig src_cfg = {};
  src_cfg.source_type    = "uhd";
  src_cfg.source_module  = uhd_source_module_path;
  src_cfg.source_params  = source_params;
  src_cfg.sample_rate    = sample_rate;
  src_cfg.nof_channels   = 1;
  src_cfg.channels.resize(1);
  src_cfg.channels[0].rx_frequency = start_freq;
  src_cfg.channels[0].tx_frequency = start_freq;
  src_cfg.channels[0].rx_gain      = rx_gain;
  src_cfg.channels[0].tx_gain      = 0;

  create_source_t create_source = load_source(uhd_source_module_path);
  Source*         source        = create_source(src_cfg);

  srsran::srsran_band_helper band_helper;
  srsran_ssb_pattern_t       ssb_pattern = band_helper.get_ssb_pattern(band, scs);
  srsran_duplex_mode_t       duplex_mode = band_helper.get_duplex_mode(band);

  auto sync_raster = band_helper.get_sync_raster(band, scs);
  if (!sync_raster.valid()) {
    logger.error("Invalid band %u or SCS", band);
    source->close();
    return 1;
  }

  uint32_t cells_found = 0;

  /* ------------------------------------------------------------------ *
   * Outer loop: walk every raster frequency in [start_freq, stop_freq] *
   * ------------------------------------------------------------------ */
  while (!sync_raster.end() && !stop_flag.load()) {
    double ssb_freq = sync_raster.get_frequency();
    if (ssb_freq < start_freq) {
      sync_raster.next();
      continue;
    }
    if (ssb_freq > stop_freq) {
      break;
    }

    /* ---------- Phase 1: detect SSB ---------- */
    logger.info("Scanning %.3f MHz", ssb_freq / 1e6);
    srsran_mib_nr_t found_mib  = {};
    uint32_t        found_N_id = 0;
    if (!scan_ssb(source, ssb_freq, logger, ssb_pattern, duplex_mode, rounds, found_mib, found_N_id)) {
      sync_raster.next();
      continue;
    }

    /* ---------- Phase 2: compute CORESET 0 ---------- */
    double   coreset0_freq    = 0.0;
    uint32_t coreset0_nof_prb = 0;
    if (!compute_coreset0(found_mib, ssb_freq, scs, logger, coreset0_freq, coreset0_nof_prb)) {
      logger.error("Failed to compute CORESET 0, skipping %.3f MHz", ssb_freq / 1e6);
      sync_raster.next();
      continue;
    }

    /* ---------- Phase 3: syncer + worker — decode SIB1 ---------- */
    config.ssb_freq          = ssb_freq;
    config.dl_freq           = coreset0_freq;
    config.ul_freq           = coreset0_freq;
    config.sample_rate       = sample_rate;
    config.band              = band;
    config.scs_ssb           = scs;
    config.scs_common        = found_mib.scs_common;
    config.nof_prb           = coreset0_nof_prb;
    config.ssb_pattern       = ssb_pattern;
    config.duplex_mode       = duplex_mode;
    config.ssb_period_ms     = 20;
    config.slot_per_subframe = 1u << (uint32_t)scs;
    config.ssb_period        = config.ssb_period_ms * config.slot_per_subframe;
    config.offset_to_carrier = 0;
    config.nof_channels      = 1;
    config.channels.resize(1);
    config.channels[0].rx_frequency = coreset0_freq;
    config.channels[0].tx_frequency = coreset0_freq;
    config.channels[0].rx_gain      = rx_gain;
    config.channels[0].tx_gain      = 0;
    config.channels[0].enabled      = true;
    config.pool_size                = 24;
    config.enable_gpu               = false;
    config.enable_recorder          = false;
    config.source_type              = "uhd";
    config.source_params            = source_params;
    config.source_module            = uhd_source_module_path;

    /* Retune SDR: center on CORESET 0 so both SSB and SIB1 PDCCH are in-band */
    source->set_rx_freq(coreset0_freq);

    /* Reset per-cell state */
    cell_done.store(false);
    cell_found_flag.store(false);
    bc_worker = nullptr;
    /* Drain any leftover tasks from a previous iteration */
    while (task_queue.retrieve_non_blocking()) {
    }

    syncer_args_t syncer_args = {
        .srate       = sample_rate,
        .scs         = scs,
        .dl_freq     = coreset0_freq,
        .ssb_freq    = ssb_freq,
        .pattern     = ssb_pattern,
        .duplex_mode = duplex_mode,
    };

    Syncer syncer(syncer_args, source, config);
    syncer.init();

    syncer.on_cell_found = [&, ssb_freq](srsran_mib_nr_t& syncer_mib, uint32_t ncellid) {
      logger.info("Syncer confirmed cell: id=%u, starting SIB1 decode", ncellid);
      bc_worker = std::make_shared<BroadCastWorker>(config);
      /* Capture ncellid and syncer_mib fields by value — on_sib1_found is called
         asynchronously after on_cell_found returns, so the parameters must not
         be referenced by address. */
      bc_worker->on_sib1_found = [&, ncellid, ssb_freq](asn1::rrc_nr_r17::sib1_s& sib1) {
        logger.info(GREEN "SIB1 decoded" RESET);
        asn1::json_writer jw;
        sib1.to_json(jw);
        logger.info("=== SIB1 ===\n%s\n", jw.to_string().c_str());
        logger.info("Detected Cell ID: %u", ncellid);
        srsran::srsran_band_helper bh;
        if (sib1.serving_cell_cfg_common_present) {
          for (const auto& band_info : sib1.serving_cell_cfg_common.dl_cfg_common.freq_info_dl.freq_band_list) {
            if (band_info.freq_band_ind_nr_present) {
              logger.info("Band info: band=%u scs_ssb=%s scs_common=%s",
                          band_info.freq_band_ind_nr,
                          srsran_subcarrier_spacing_to_str(scs),
                          srsran_subcarrier_spacing_to_str(found_mib.scs_common));
            }
          }
          uint32_t offset_to_point_a = sib1.serving_cell_cfg_common.dl_cfg_common.freq_info_dl.offset_to_point_a;
          if (sib1.serving_cell_cfg_common.dl_cfg_common.freq_info_dl.scs_specific_carrier_list.size() > 0) {
            uint32_t nof_prbs =
                sib1.serving_cell_cfg_common.dl_cfg_common.freq_info_dl.scs_specific_carrier_list[0].carrier_bw;
            double center_freq = ssb_freq - (15000 << (uint32_t)scs) * 12 * 10 - found_mib.ssb_offset * 15e3 -
                                 offset_to_point_a * 12 * 15e3 +
                                 (nof_prbs * 12.0 * (15000 << (uint32_t)found_mib.scs_common)) / 2.0;
            logger.info("Nof PRBs: %u", nof_prbs);
            logger.info("DL center frequency: %.3f MHz", center_freq / 1e6);
            logger.info("DL ARFCN: %u", bh.freq_to_nr_arfcn(center_freq));
            logger.info("SSB frequency: %.3f MHz", ssb_freq / 1e6);
            logger.info("SSB ARFCN: %u", bh.freq_to_nr_arfcn(ssb_freq));
          }
          if (sib1.serving_cell_cfg_common.ul_cfg_common.freq_info_ul.absolute_freq_point_a_present) {
            uint32_t absolute_freq_point_a_ul =
                sib1.serving_cell_cfg_common.ul_cfg_common.freq_info_ul.absolute_freq_point_a;
            if (sib1.serving_cell_cfg_common.ul_cfg_common.freq_info_ul.scs_specific_carrier_list.size() > 0) {
              uint32_t nof_prbs_ul =
                  sib1.serving_cell_cfg_common.ul_cfg_common.freq_info_ul.scs_specific_carrier_list[0].carrier_bw;
              double ul_point_a_freq = bh.nr_arfcn_to_freq(absolute_freq_point_a_ul);
              double center_freq_ul =
                  ul_point_a_freq + (nof_prbs_ul * 12.0 * (15000 << (uint32_t)found_mib.scs_common)) / 2.0;
              logger.info("UL center frequency: %.3f MHz", center_freq_ul / 1e6);
            }
          }
          logger.info("SSB periodicity: %s", sib1.serving_cell_cfg_common.ssb_periodicity_serving_cell.to_string());
        }
        cell_done.store(true); /* signal main loop to move on */
      };
      bc_worker->apply_config_from_mib(syncer_mib, ncellid);
      cell_found_flag.store(true);
    };

    syncer.error_handler = [&]() {
      logger.info("Source error or end of stream at %.3f MHz", ssb_freq / 1e6);
      cell_done.store(true);
    };
    syncer.publish_subframe = std::bind(push_new_task, std::placeholders::_1);

    std::thread decoder(decoder_worker);
    syncer.start(0);

    /* Wait for SIB1 decode, source error, or SIGINT.
       A 30-second timeout guards against cells where the syncer cannot
       re-acquire after the initial SSB detection. */
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
    while (!cell_done.load() && !stop_flag.load()) {
      if (std::chrono::steady_clock::now() >= deadline) {
        logger.info("Timeout waiting for SIB1 at %.3f MHz", ssb_freq / 1e6);
        break;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    syncer.stop();
    syncer.wait_thread_finish();
    task_queue.push(nullptr); /* wake and stop the decoder thread */
    decoder.join();

    if (cell_found_flag.load()) {
      cells_found++;
    }

    /* Advance to the next raster frequency regardless of outcome */
    sync_raster.next();
  }

  source->close();
  logger.info("Scan complete. Cells decoded: %u", cells_found);
  return (cells_found > 0) ? 0 : 1;
}
