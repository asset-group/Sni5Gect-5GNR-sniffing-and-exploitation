#include "shadower/utils/rrc_nr_utils_r17.h"
#include "srsran/common/band_helper.h"

bool make_phy_common_time_ra(const asn1::rrc_nr_r17::pdsch_time_domain_res_alloc_s& time_domain_alloc,
                             srsran_sch_time_ra_t*                                  time_ra)
{
  srsran_sch_time_ra_t srsran_sch_time_ra = {};
  srsran_sch_time_ra.sliv                 = time_domain_alloc.start_symbol_and_len;
  switch (time_domain_alloc.map_type) {
    case asn1::rrc_nr_r17::pdsch_time_domain_res_alloc_s::map_type_opts::type_a:
      srsran_sch_time_ra.mapping_type = srsran_sch_mapping_type_A;
      break;
    case asn1::rrc_nr_r17::pdsch_time_domain_res_alloc_s::map_type_opts::type_b:
      srsran_sch_time_ra.mapping_type = srsran_sch_mapping_type_B;
      break;
    default:
      asn1::log_warning("Invalid option for map_type: %s", time_domain_alloc.map_type.to_string());
      break;
  }
  if (time_domain_alloc.k0_present) {
    srsran_sch_time_ra.k = time_domain_alloc.k0;
  } else {
    srsran_sch_time_ra.k = 0;
  }
  *time_ra = srsran_sch_time_ra;
  return true;
}

bool make_phy_common_time_ra(const asn1::rrc_nr_r17::pusch_time_domain_res_alloc_s& time_domain_alloc,
                             srsran_sch_time_ra_t*                                  time_ra)
{
  srsran_sch_time_ra_t srsran_sch_time_ra = {};
  srsran_sch_time_ra.sliv                 = time_domain_alloc.start_symbol_and_len;
  switch (time_domain_alloc.map_type) {
    case asn1::rrc_nr_r17::pusch_time_domain_res_alloc_s::map_type_opts::type_a:
      srsran_sch_time_ra.mapping_type = srsran_sch_mapping_type_A;
      break;
    case asn1::rrc_nr_r17::pusch_time_domain_res_alloc_s::map_type_opts::type_b:
      srsran_sch_time_ra.mapping_type = srsran_sch_mapping_type_B;
      break;
    default:
      asn1::log_warning("Invalid option for map_type: %s", time_domain_alloc.map_type.to_string());
      break;
  }
  if (time_domain_alloc.k2_present) {
    srsran_sch_time_ra.k = time_domain_alloc.k2;
  } else {
    srsran_sch_time_ra.k = 0;
  }
  *time_ra = srsran_sch_time_ra;
  return true;
}

bool fill_pdsch_cfg_common(asn1::rrc_nr_r17::pdsch_cfg_common_s& pdsch_cfg, srsran_sch_hl_cfg_nr_t* pdsch)
{
  for (uint32_t i = 0; i < pdsch_cfg.pdsch_time_domain_alloc_list.size(); i++) {
    srsran_sch_time_ra_t time_ra;
    if (!make_phy_common_time_ra(pdsch_cfg.pdsch_time_domain_alloc_list[i], &time_ra)) {
      asn1::log_warning("Failed to convert PDSCH time domain resource allocation to PHY format");
      return false;
    }
    pdsch->common_time_ra[i]  = time_ra;
    pdsch->nof_common_time_ra = i + 1;
  }
  return true;
}

bool fill_pusch_cfg_common(asn1::rrc_nr_r17::pusch_cfg_common_s& pusch_cfg, srsran_sch_hl_cfg_nr_t* pusch)
{
  for (uint32_t i = 0; i < pusch_cfg.pusch_time_domain_alloc_list.size(); i++) {
    srsran_sch_time_ra_t time_ra;
    if (!make_phy_common_time_ra(pusch_cfg.pusch_time_domain_alloc_list[i], &time_ra)) {
      asn1::log_warning("Failed to convert PUSCH time domain resource allocation to PHY format");
      return false;
    }
    pusch->common_time_ra[i]  = time_ra;
    pusch->nof_common_time_ra = i + 1;
  }
  return true;
}

bool make_phy_tdd_cfg(const asn1::rrc_nr_r17::tdd_ul_dl_cfg_common_s& tdd_ul_dl_cfg_common,
                      srsran_duplex_config_nr_t*                      in_srsran_duplex_config_nr)
{
  srsran_duplex_config_nr_t srsran_duplex_config_nr = {};
  srsran_duplex_config_nr.mode                      = SRSRAN_DUPLEX_MODE_TDD;

  switch (tdd_ul_dl_cfg_common.pattern1.dl_ul_tx_periodicity) {
    case asn1::rrc_nr_r17::tdd_ul_dl_pattern_s::dl_ul_tx_periodicity_opts::ms1:
      srsran_duplex_config_nr.tdd.pattern1.period_ms = 1;
      break;
    case asn1::rrc_nr_r17::tdd_ul_dl_pattern_s::dl_ul_tx_periodicity_opts::ms2:
      srsran_duplex_config_nr.tdd.pattern1.period_ms = 2;
      break;
    case asn1::rrc_nr_r17::tdd_ul_dl_pattern_s::dl_ul_tx_periodicity_opts::ms2p5:
      srsran_duplex_config_nr.tdd.pattern1.period_ms = 2.5f;
      break;
    case asn1::rrc_nr_r17::tdd_ul_dl_pattern_s::dl_ul_tx_periodicity_opts::ms5:
      srsran_duplex_config_nr.tdd.pattern1.period_ms = 5;
      break;
    case asn1::rrc_nr_r17::tdd_ul_dl_pattern_s::dl_ul_tx_periodicity_opts::ms10:
      srsran_duplex_config_nr.tdd.pattern1.period_ms = 10;
      break;

    case asn1::rrc_nr_r17::tdd_ul_dl_pattern_s::dl_ul_tx_periodicity_opts::ms1p25:
    case asn1::rrc_nr_r17::tdd_ul_dl_pattern_s::dl_ul_tx_periodicity_opts::ms0p5:
    case asn1::rrc_nr_r17::tdd_ul_dl_pattern_s::dl_ul_tx_periodicity_opts::ms0p625:
    default:
      asn1::log_warning("Invalid option for dl_ul_tx_periodicity_opts %s",
                        tdd_ul_dl_cfg_common.pattern1.dl_ul_tx_periodicity.to_string());
      return false;
  }
  srsran_duplex_config_nr.tdd.pattern1.nof_dl_slots   = tdd_ul_dl_cfg_common.pattern1.nrof_dl_slots;
  srsran_duplex_config_nr.tdd.pattern1.nof_dl_symbols = tdd_ul_dl_cfg_common.pattern1.nrof_dl_symbols;
  srsran_duplex_config_nr.tdd.pattern1.nof_ul_slots   = tdd_ul_dl_cfg_common.pattern1.nrof_ul_slots;
  srsran_duplex_config_nr.tdd.pattern1.nof_ul_symbols = tdd_ul_dl_cfg_common.pattern1.nrof_ul_symbols;

  if (tdd_ul_dl_cfg_common.pattern2_present) {
    switch (tdd_ul_dl_cfg_common.pattern2.dl_ul_tx_periodicity) {
      case asn1::rrc_nr_r17::tdd_ul_dl_pattern_s::dl_ul_tx_periodicity_opts::ms1:
        srsran_duplex_config_nr.tdd.pattern2.period_ms = 1;
        break;
      case asn1::rrc_nr_r17::tdd_ul_dl_pattern_s::dl_ul_tx_periodicity_opts::ms2:
        srsran_duplex_config_nr.tdd.pattern2.period_ms = 2;
        break;
      case asn1::rrc_nr_r17::tdd_ul_dl_pattern_s::dl_ul_tx_periodicity_opts::ms2p5:
        srsran_duplex_config_nr.tdd.pattern2.period_ms = 2.5f;
        break;
      case asn1::rrc_nr_r17::tdd_ul_dl_pattern_s::dl_ul_tx_periodicity_opts::ms5:
        srsran_duplex_config_nr.tdd.pattern2.period_ms = 5;
        break;
      case asn1::rrc_nr_r17::tdd_ul_dl_pattern_s::dl_ul_tx_periodicity_opts::ms10:
        srsran_duplex_config_nr.tdd.pattern2.period_ms = 10;
        break;

      case asn1::rrc_nr_r17::tdd_ul_dl_pattern_s::dl_ul_tx_periodicity_opts::ms1p25:
      case asn1::rrc_nr_r17::tdd_ul_dl_pattern_s::dl_ul_tx_periodicity_opts::ms0p5:
      case asn1::rrc_nr_r17::tdd_ul_dl_pattern_s::dl_ul_tx_periodicity_opts::ms0p625:
      default:
        asn1::log_warning("Invalid option for pattern2 dl_ul_tx_periodicity_opts %s",
                          tdd_ul_dl_cfg_common.pattern2.dl_ul_tx_periodicity.to_string());
        return false;
    }

    srsran_duplex_config_nr.tdd.pattern2.nof_dl_slots   = tdd_ul_dl_cfg_common.pattern2.nrof_dl_slots;
    srsran_duplex_config_nr.tdd.pattern2.nof_dl_symbols = tdd_ul_dl_cfg_common.pattern2.nrof_dl_symbols;
    srsran_duplex_config_nr.tdd.pattern2.nof_ul_slots   = tdd_ul_dl_cfg_common.pattern2.nrof_ul_slots;
    srsran_duplex_config_nr.tdd.pattern2.nof_ul_symbols = tdd_ul_dl_cfg_common.pattern2.nrof_ul_symbols;
  }
  // Copy and return struct
  *in_srsran_duplex_config_nr = srsran_duplex_config_nr;
  return true;
}

template <class bitstring_t>
static inline void make_ssb_positions_in_burst(const bitstring_t&                           ans1_position_in_burst,
                                               std::array<bool, SRSRAN_SSB_NOF_CANDIDATES>& position_in_burst)
{
  for (uint32_t i = 0; i < SRSRAN_SSB_NOF_CANDIDATES; i++) {
    if (i < ans1_position_in_burst.length()) {
      position_in_burst[i] = ans1_position_in_burst.get(ans1_position_in_burst.length() - 1 - i);
    } else {
      position_in_burst[i] = false;
    }
  }
}

void fill_phy_ssb_cfg(const asn1::rrc_nr_r17::serving_cell_cfg_common_sib_s& serv_cell_cfg,
                      srsran::phy_cfg_nr_t::ssb_cfg_t*                       out_ssb)
{
  out_ssb->periodicity_ms = serv_cell_cfg.ssb_periodicity_serving_cell.to_number();

  if (serv_cell_cfg.ssb_positions_in_burst.group_presence_present) {
    make_ssb_positions_in_burst(serv_cell_cfg.ssb_positions_in_burst.group_presence, out_ssb->position_in_burst);
  } else {
    make_ssb_positions_in_burst(serv_cell_cfg.ssb_positions_in_burst.in_one_group, out_ssb->position_in_burst);
  }
}

bool fill_phy_pusch_cfg_common(const asn1::rrc_nr_r17::pusch_cfg_common_s& pusch_cfg, srsran_sch_hl_cfg_nr_t* pusch)
{
  for (uint32_t i = 0; i < pusch_cfg.pusch_time_domain_alloc_list.size(); i++) {
    srsran_sch_time_ra_t common_time_ra;
    if (make_phy_common_time_ra(pusch_cfg.pusch_time_domain_alloc_list[i], &common_time_ra) == true) {
      pusch->common_time_ra[i]  = common_time_ra;
      pusch->nof_common_time_ra = i + 1;
    } else {
      asn1::log_warning("Warning while building common_time_ra structure");
      return false;
    }
  }
  if (pusch_cfg.group_hop_enabled_transform_precoding_present) {
    pusch->enable_hopping = true;
  }
  return true;
}

void fill_phy_pucch_cfg_common(const asn1::rrc_nr_r17::pucch_cfg_common_s& pucch_cfg,
                               srsran_pucch_nr_common_cfg_t*               pucch)
{
  if (pucch_cfg.pucch_res_common_present) {
    pucch->resource_common = pucch_cfg.pucch_res_common;
  }
  if (pucch_cfg.hop_id_present) {
    pucch->hopping_id_present = true;
    pucch->hopping_id         = pucch_cfg.hop_id;
  }
  if (pucch_cfg.p0_nominal_present) {
    pucch->p0_nominal = pucch_cfg.p0_nominal;
  }

  switch (pucch_cfg.pucch_group_hop) {
    case asn1::rrc_nr_r17::pucch_cfg_common_s::pucch_group_hop_opts::enable:
      pucch->group_hopping = SRSRAN_PUCCH_NR_GROUP_HOPPING_ENABLE;
      break;
    case asn1::rrc_nr_r17::pucch_cfg_common_s::pucch_group_hop_opts::disable:
      pucch->group_hopping = SRSRAN_PUCCH_NR_GROUP_HOPPING_DISABLE;
      break;
    default:
      pucch->group_hopping = SRSRAN_PUCCH_NR_GROUP_HOPPING_NEITHER;
      break;
  }
}

bool make_phy_rach_cfg(const asn1::rrc_nr_r17::rach_cfg_common_s& asn1_type,
                       srsran_duplex_mode_t                       duplex_mode,
                       srsran_prach_cfg_t*                        prach_cfg)
{
  prach_cfg->is_nr            = true;
  prach_cfg->config_idx       = asn1_type.rach_cfg_generic.prach_cfg_idx;
  prach_cfg->zero_corr_zone   = (uint32_t)asn1_type.rach_cfg_generic.zero_correlation_zone_cfg;
  prach_cfg->num_ra_preambles = 64;
  if (asn1_type.total_nof_ra_preambs_present) {
    prach_cfg->num_ra_preambles = asn1_type.total_nof_ra_preambs;
  }
  prach_cfg->hs_flag    = false; // Hard-coded
  prach_cfg->tdd_config = {};
  if (duplex_mode == SRSRAN_DUPLEX_MODE_TDD) {
    prach_cfg->tdd_config.configured = true;
  }

  // As the current PRACH is based on LTE, the freq-offset shall be subtracted 1 for aligning with NR bandwidth
  // For example. A 52 PRB cell with an freq_offset of 1 will match a LTE 50 PRB cell with freq_offset of 0
  prach_cfg->freq_offset = (uint32_t)asn1_type.rach_cfg_generic.msg1_freq_start;
  // if (prach_cfg->freq_offset == 0) {
  // asn1::log_error("PRACH freq offset must be at least one");
  // return false;
  // }

  switch (asn1_type.prach_root_seq_idx.type().value) {
    case asn1::rrc_nr_r17::rach_cfg_common_s::prach_root_seq_idx_c_::types_opts::l839:
      prach_cfg->root_seq_idx = (uint32_t)asn1_type.prach_root_seq_idx.l839();
      break;
    case asn1::rrc_nr_r17::rach_cfg_common_s::prach_root_seq_idx_c_::types_opts::l139:
      prach_cfg->root_seq_idx = (uint32_t)asn1_type.prach_root_seq_idx.l139();
      break;
    default:
      asn1::log_error("Not-implemented option for prach_root_seq_idx type %s",
                      asn1_type.prach_root_seq_idx.type().to_string());
      return false;
  }
  if (asn1_type.msg3_transform_precoder_present) {
    prach_cfg->enable_msg3_transform_precoder = true;
  }
  return true;
};

static inline srsran_subcarrier_spacing_t
make_subcarrier_spacing(const asn1::rrc_nr_r17::subcarrier_spacing_e& asn1_scs)
{
  switch (asn1_scs) {
    case asn1::rrc_nr_r17::subcarrier_spacing_opts::options::khz15:
      return srsran_subcarrier_spacing_15kHz;
    case asn1::rrc_nr_r17::subcarrier_spacing_opts::options::khz30:
      return srsran_subcarrier_spacing_30kHz;
    case asn1::rrc_nr_r17::subcarrier_spacing_opts::options::khz60:
      return srsran_subcarrier_spacing_60kHz;
    case asn1::rrc_nr_r17::subcarrier_spacing_opts::options::khz120:
      return srsran_subcarrier_spacing_120kHz;
    case asn1::rrc_nr_r17::subcarrier_spacing_opts::options::khz240:
      return srsran_subcarrier_spacing_240kHz;
    case asn1::rrc_nr_r17::subcarrier_spacing_opts::spare1:
    case asn1::rrc_nr_r17::subcarrier_spacing_opts::nulltype:
    default:
      asn1::log_warning("Not supported subcarrier spacing ");
      break;
  }
  return srsran_subcarrier_spacing_invalid;
}

void fill_phy_carrier_cfg(const asn1::rrc_nr_r17::serving_cell_cfg_common_sib_s& serv_cell_cfg,
                          srsran_carrier_nr_t*                                   out_carrier_nr)
{
  // TODO: Currently ony one carrier is supported
  auto& freq_info_dl                = serv_cell_cfg.dl_cfg_common.freq_info_dl;
  out_carrier_nr->offset_to_carrier = freq_info_dl.scs_specific_carrier_list[0].offset_to_carrier;
  out_carrier_nr->scs     = make_subcarrier_spacing(freq_info_dl.scs_specific_carrier_list[0].subcarrier_spacing);
  out_carrier_nr->nof_prb = freq_info_dl.scs_specific_carrier_list[0].carrier_bw;

  auto& freq_info_ul = serv_cell_cfg.ul_cfg_common.freq_info_ul;
  if (freq_info_ul.absolute_freq_point_a_present) {
    srsran::srsran_band_helper bands;
    out_carrier_nr->ul_center_frequency_hz = bands.get_center_freq_from_abs_freq_point_a(
        freq_info_ul.scs_specific_carrier_list[0].carrier_bw, freq_info_ul.absolute_freq_point_a);
  }
}

bool make_phy_coreset_cfg(const asn1::rrc_nr_r17::coreset_s& coreset, srsran_coreset_t* in_srsran_coreset)
{
  srsran_coreset_t srsran_coreset = {};
  srsran_coreset.id               = coreset.coreset_id;
  switch (coreset.precoder_granularity) {
    case asn1::rrc_nr_r17::coreset_s::precoder_granularity_opts::same_as_reg_bundle:
      srsran_coreset.precoder_granularity = srsran_coreset_precoder_granularity_reg_bundle;
      break;
    case asn1::rrc_nr_r17::coreset_s::precoder_granularity_opts::all_contiguous_rbs:
      srsran_coreset.precoder_granularity = srsran_coreset_precoder_granularity_contiguous;
    default:
      asn1::log_warning("Invalid option for precoder_granularity %s", coreset.precoder_granularity.to_string());
      return false;
  };

  switch (coreset.cce_reg_map_type.type()) {
    case asn1::rrc_nr_r17::coreset_s::cce_reg_map_type_c_::types_opts::options::interleaved:
      srsran_coreset.mapping_type = srsran_coreset_mapping_type_interleaved;
      break;
    case asn1::rrc_nr_r17::coreset_s::cce_reg_map_type_c_::types_opts::options::non_interleaved:
      srsran_coreset.mapping_type = srsran_coreset_mapping_type_non_interleaved;
      break;
    default:
      asn1::log_warning("Invalid option for cce_reg_map_type: %s", coreset.cce_reg_map_type.type().to_string());
      return false;
  }
  srsran_coreset.duration = coreset.dur;
  for (uint32_t i = 0; i < SRSRAN_CORESET_FREQ_DOMAIN_RES_SIZE; i++) {
    srsran_coreset.freq_resources[i] = coreset.freq_domain_res.get(SRSRAN_CORESET_FREQ_DOMAIN_RES_SIZE - 1 - i);
  }
  if (coreset.pdcch_dmrs_scrambling_id_present) {
    srsran_coreset.dmrs_scrambling_id         = coreset.pdcch_dmrs_scrambling_id;
    srsran_coreset.dmrs_scrambling_id_present = true;
  }
  *in_srsran_coreset = srsran_coreset;
  return true;
}

bool make_phy_search_space_cfg(const asn1::rrc_nr_r17::search_space_s& search_space,
                               srsran_search_space_t*                  in_srsran_search_space)
{
  srsran_search_space_t srsran_search_space = {};
  srsran_search_space.id                    = search_space.search_space_id;
  if (not search_space.coreset_id_present) {
    asn1::log_warning("coreset id option not present");
    return false;
  }
  srsran_search_space.coreset_id = search_space.coreset_id;

  srsran_search_space.duration = 1;
  if (search_space.dur_present) {
    srsran_search_space.duration = search_space.dur;
  }

  if (not search_space.nrof_candidates_present) {
    asn1::log_warning("nrof_candidates_present option not present");
    return false;
  }
  srsran_search_space.nof_candidates[0] = search_space.nrof_candidates.aggregation_level1.to_number();
  srsran_search_space.nof_candidates[1] = search_space.nrof_candidates.aggregation_level2.to_number();
  srsran_search_space.nof_candidates[2] = search_space.nrof_candidates.aggregation_level4.to_number();
  srsran_search_space.nof_candidates[3] = search_space.nrof_candidates.aggregation_level8.to_number();
  srsran_search_space.nof_candidates[4] = search_space.nrof_candidates.aggregation_level16.to_number();

  // Parse monitoringSymbolsWithinSlot
  if (search_space.monitoring_symbols_within_slot_present) {
    uint64_t raw                                       = search_space.monitoring_symbols_within_slot.to_number();
    srsran_search_space.monitoring_symbols_within_slot = 0;
    for (uint32_t s = 0; s < 14; s++) {
      if ((raw >> (13 - s)) & 1) {
        srsran_search_space.monitoring_symbols_within_slot |= (1ULL << s);
      }
    }
    srsran_search_space.monitoring_symbols_within_slot_present = true;
  } else {
    srsran_search_space.monitoring_symbols_within_slot         = 0;
    srsran_search_space.monitoring_symbols_within_slot_present = false;
  }

  if (not search_space.search_space_type_present) {
    asn1::log_warning("search_space_type option not present");
    return false;
  }
  switch (search_space.search_space_type.type()) {
    case asn1::rrc_nr_r17::search_space_s::search_space_type_c_::types_opts::options::common:
      srsran_search_space.type = srsran_search_space_type_common_3;

      // dci-Format0-0-AndFormat1-0
      // If configured, the UE monitors the DCI formats 0_0 and 1_0 according to TS 38.213 [13], clause 10.1.
      if (search_space.search_space_type.common().dci_format0_0_and_format1_0_present) {
        srsran_search_space.formats[srsran_search_space.nof_formats++] = srsran_dci_format_nr_0_0;
        srsran_search_space.formats[srsran_search_space.nof_formats++] = srsran_dci_format_nr_1_0;
      }

      // dci-Format2-0
      // If configured, UE monitors the DCI format 2_0 according to TS 38.213 [13], clause 10.1, 11.1.1.
      if (search_space.search_space_type.common().dci_format2_0_present) {
        srsran_search_space.formats[srsran_search_space.nof_formats++] = srsran_dci_format_nr_2_0;
      }

      // dci-Format2-1
      // If configured, UE monitors the DCI format 2_1 according to TS 38.213 [13], clause 10.1, 11.2.
      if (search_space.search_space_type.common().dci_format2_1_present) {
        srsran_search_space.formats[srsran_search_space.nof_formats++] = srsran_dci_format_nr_2_1;
      }

      // dci-Format2-2
      // If configured, UE monitors the DCI format 2_2 according to TS 38.213 [13], clause 10.1, 11.3.
      if (search_space.search_space_type.common().dci_format2_2_present) {
        srsran_search_space.formats[srsran_search_space.nof_formats++] = srsran_dci_format_nr_2_2;
      }

      // dci-Format2-3
      // If configured, UE monitors the DCI format 2_3 according to TS 38.213 [13], clause 10.1, 11.4
      if (search_space.search_space_type.common().dci_format2_3_present) {
        srsran_search_space.formats[srsran_search_space.nof_formats++] = srsran_dci_format_nr_2_3;
      }

      break;
    case asn1::rrc_nr_r17::search_space_s::search_space_type_c_::types_opts::options::ue_specific:
      srsran_search_space.type = srsran_search_space_type_ue;
      switch (search_space.search_space_type.ue_specific().dci_formats.value) {
        case asn1::rrc_nr_r17::search_space_s::search_space_type_c_::ue_specific_s_::dci_formats_e_::
            formats0_neg0_and_neg1_neg0:
          srsran_search_space.formats[srsran_search_space.nof_formats++] = srsran_dci_format_nr_0_0;
          srsran_search_space.formats[srsran_search_space.nof_formats++] = srsran_dci_format_nr_1_0;
          break;
        case asn1::rrc_nr_r17::search_space_s::search_space_type_c_::ue_specific_s_::dci_formats_e_::
            formats0_neg1_and_neg1_neg1:
          srsran_search_space.formats[srsran_search_space.nof_formats++] = srsran_dci_format_nr_0_1;
          srsran_search_space.formats[srsran_search_space.nof_formats++] = srsran_dci_format_nr_1_1;
          break;
        default:
          asn1::log_warning("Invalid option for ue_specific dci_formats: %s",
                            search_space.search_space_type.ue_specific().dci_formats.to_string());
          return false;
      }
      break;
    default:
      asn1::log_warning("Invalid option for search_space_type %s", search_space.search_space_type.type().to_string());
      return false;
  }
  // Copy struct and return value
  *in_srsran_search_space = srsran_search_space;
  return true;
}

bool fill_phy_pdcch_cfg_common(const asn1::rrc_nr_r17::pdcch_cfg_common_s& pdcch_cfg, srsran_pdcch_cfg_nr_t* pdcch)
{
  if (pdcch_cfg.common_coreset_present) {
    pdcch->coreset_present[pdcch_cfg.common_coreset.coreset_id] = true;
    make_phy_coreset_cfg(pdcch_cfg.common_coreset, &pdcch->coreset[pdcch_cfg.common_coreset.coreset_id]);
  }
  for (const asn1::rrc_nr_r17::search_space_s& ss : pdcch_cfg.common_search_space_list) {
    pdcch->search_space_present[ss.search_space_id] = true;
    if (not make_phy_search_space_cfg(ss, &pdcch->search_space[ss.search_space_id])) {
      asn1::log_error("Failed to convert SearchSpace Configuration");
      return false;
    }
    if (pdcch_cfg.ra_search_space_present and pdcch_cfg.ra_search_space == ss.search_space_id) {
      pdcch->ra_search_space_present     = true;
      pdcch->ra_search_space             = pdcch->search_space[ss.search_space_id];
      pdcch->ra_search_space.type        = srsran_search_space_type_common_1;
      pdcch->ra_search_space.nof_formats = 1;
      pdcch->ra_search_space.formats[0]  = srsran_dci_format_nr_1_0;
    }
  }
  return true;
}

bool fill_phy_pdsch_cfg_common(const asn1::rrc_nr_r17::pdsch_cfg_common_s& pdsch_cfg, srsran_sch_hl_cfg_nr_t* pdsch)
{
  for (uint32_t i = 0; i < pdsch_cfg.pdsch_time_domain_alloc_list.size(); i++) {
    srsran_sch_time_ra_t common_time_ra;
    if (make_phy_common_time_ra(pdsch_cfg.pdsch_time_domain_alloc_list[i], &common_time_ra) == true) {
      pdsch->common_time_ra[i]  = common_time_ra;
      pdsch->nof_common_time_ra = i + 1;
    } else {
      asn1::log_warning("Warning while building common_time_ra structure");
      return false;
    }
  }
  return true;
}

bool make_phy_pdsch_alloc_type(const asn1::rrc_nr_r17::pdsch_cfg_s& pdsch_cfg,
                               srsran_resource_alloc_t*             in_srsran_resource_alloc)
{
  srsran_resource_alloc_t srsran_resource_alloc = {};

  switch (pdsch_cfg.res_alloc) {
    case asn1::rrc_nr_r17::pdsch_cfg_s::res_alloc_e_::res_alloc_type0:
      srsran_resource_alloc = srsran_resource_alloc_type0;
      break;
    case asn1::rrc_nr_r17::pdsch_cfg_s::res_alloc_e_::res_alloc_type1:
      srsran_resource_alloc = srsran_resource_alloc_type1;
      break;
    case asn1::rrc_nr_r17::pdsch_cfg_s::res_alloc_e_::dyn_switch:
      srsran_resource_alloc = srsran_resource_alloc_dynamic;
      break;
    default:
      asn1::log_warning("Invalid option for pusch::resource_alloc %s", pdsch_cfg.res_alloc.to_string());
      return false;
  }
  *in_srsran_resource_alloc = srsran_resource_alloc;
  return true;
}

bool make_phy_dmrs_dl_additional_pos(const asn1::rrc_nr_r17::dmrs_dl_cfg_s& dmrs_dl_cfg,
                                     srsran_dmrs_sch_add_pos_t*             in_srsran_dmrs_sch_add_pos)
{
  srsran_dmrs_sch_add_pos_t srsran_dmrs_sch_add_pos = {};
  if (not dmrs_dl_cfg.dmrs_add_position_present) {
    asn1::log_warning("dmrs_add_position option not present");
  }

  switch (dmrs_dl_cfg.dmrs_add_position) {
    case asn1::rrc_nr_r17::dmrs_dl_cfg_s::dmrs_add_position_opts::pos0:
      srsran_dmrs_sch_add_pos = srsran_dmrs_sch_add_pos_0;
      break;
    case asn1::rrc_nr_r17::dmrs_dl_cfg_s::dmrs_add_position_opts::pos1:
      srsran_dmrs_sch_add_pos = srsran_dmrs_sch_add_pos_1;
      break;
    case asn1::rrc_nr_r17::dmrs_dl_cfg_s::dmrs_add_position_opts::pos3:
      srsran_dmrs_sch_add_pos = srsran_dmrs_sch_add_pos_3;
      break;
    default:
      srsran_dmrs_sch_add_pos = srsran_dmrs_sch_add_pos_2;
      break;
  }
  *in_srsran_dmrs_sch_add_pos = srsran_dmrs_sch_add_pos;
  return true;
}

bool make_phy_dmrs_ul_additional_pos(const asn1::rrc_nr_r17::dmrs_ul_cfg_s& dmrs_ul_cfg,
                                     srsran_dmrs_sch_add_pos_t*             in_srsran_dmrs_sch_add_pos)
{
  srsran_dmrs_sch_add_pos_t srsran_dmrs_sch_add_pos = {};
  if (not dmrs_ul_cfg.dmrs_add_position_present) {
    asn1::log_warning("dmrs_add_position option not present");
  }

  switch (dmrs_ul_cfg.dmrs_add_position) {
    case asn1::rrc_nr_r17::dmrs_ul_cfg_s::dmrs_add_position_opts::pos0:
      srsran_dmrs_sch_add_pos = srsran_dmrs_sch_add_pos_0;
      break;
    case asn1::rrc_nr_r17::dmrs_ul_cfg_s::dmrs_add_position_opts::pos1:
      srsran_dmrs_sch_add_pos = srsran_dmrs_sch_add_pos_1;
      break;
    case asn1::rrc_nr_r17::dmrs_ul_cfg_s::dmrs_add_position_opts::pos3:
      srsran_dmrs_sch_add_pos = srsran_dmrs_sch_add_pos_3;
      break;
    default:
      srsran_dmrs_sch_add_pos = srsran_dmrs_sch_add_pos_2;
      break;
  }
  *in_srsran_dmrs_sch_add_pos = srsran_dmrs_sch_add_pos;
  return true;
}

bool make_phy_zp_csi_rs_resource(const asn1::rrc_nr_r17::zp_csi_rs_res_s& zp_csi_rs_res,
                                 srsran_csi_rs_zp_resource_t*             out_zp_csi_rs_resource)
{
  srsran_csi_rs_zp_resource_t zp_csi_rs_resource = {};
  zp_csi_rs_resource.id                          = zp_csi_rs_res.zp_csi_rs_res_id;
  switch (zp_csi_rs_res.res_map.freq_domain_alloc.type()) {
    case asn1::rrc_nr_r17::csi_rs_res_map_s::freq_domain_alloc_c_::types_opts::options::row1:
      zp_csi_rs_resource.resource_mapping.row = srsran_csi_rs_resource_mapping_row_1;
      for (uint32_t i = 0; i < zp_csi_rs_res.res_map.freq_domain_alloc.row1().length(); i++) {
        zp_csi_rs_resource.resource_mapping.frequency_domain_alloc[i] =
            zp_csi_rs_res.res_map.freq_domain_alloc.row1().get(zp_csi_rs_res.res_map.freq_domain_alloc.row1().length() -
                                                               1 - i);
      }
      break;
    case asn1::rrc_nr_r17::csi_rs_res_map_s::freq_domain_alloc_c_::types_opts::options::row2:
      zp_csi_rs_resource.resource_mapping.row = srsran_csi_rs_resource_mapping_row_2;
      for (uint32_t i = 0; i < zp_csi_rs_res.res_map.freq_domain_alloc.row2().length(); i++) {
        zp_csi_rs_resource.resource_mapping.frequency_domain_alloc[i] =
            zp_csi_rs_res.res_map.freq_domain_alloc.row2().get(zp_csi_rs_res.res_map.freq_domain_alloc.row2().length() -
                                                               1 - i);
      }
      break;
    case asn1::rrc_nr_r17::csi_rs_res_map_s::freq_domain_alloc_c_::types_opts::options::row4:
      zp_csi_rs_resource.resource_mapping.row = srsran_csi_rs_resource_mapping_row_4;
      for (uint32_t i = 0; i < zp_csi_rs_res.res_map.freq_domain_alloc.row4().length(); i++) {
        zp_csi_rs_resource.resource_mapping.frequency_domain_alloc[i] =
            zp_csi_rs_res.res_map.freq_domain_alloc.row4().get(zp_csi_rs_res.res_map.freq_domain_alloc.row4().length() -
                                                               1 - i);
      }
      break;
    case asn1::rrc_nr_r17::csi_rs_res_map_s::freq_domain_alloc_c_::types_opts::options::other:
      zp_csi_rs_resource.resource_mapping.row = srsran_csi_rs_resource_mapping_row_other;
      for (uint32_t i = 0; i < zp_csi_rs_res.res_map.freq_domain_alloc.other().length(); i++) {
        zp_csi_rs_resource.resource_mapping.frequency_domain_alloc[i] =
            zp_csi_rs_res.res_map.freq_domain_alloc.other().get(
                zp_csi_rs_res.res_map.freq_domain_alloc.other().length() - 1 - i);
      }
      break;
    default:
      asn1::log_warning("Invalid option for freq_domain_alloc %s",
                        zp_csi_rs_res.res_map.freq_domain_alloc.type().to_string());
      return false;
  }
  zp_csi_rs_resource.resource_mapping.nof_ports        = zp_csi_rs_res.res_map.nrof_ports.to_number();
  zp_csi_rs_resource.resource_mapping.first_symbol_idx = zp_csi_rs_res.res_map.first_ofdm_symbol_in_time_domain;
  if (zp_csi_rs_res.res_map.first_ofdm_symbol_in_time_domain2_present) {
    zp_csi_rs_resource.resource_mapping.first_symbol_idx2 = zp_csi_rs_res.res_map.first_ofdm_symbol_in_time_domain2;
  }

  switch (zp_csi_rs_res.res_map.cdm_type) {
    case asn1::rrc_nr_r17::csi_rs_res_map_s::cdm_type_opts::options::no_cdm:
      zp_csi_rs_resource.resource_mapping.cdm = srsran_csi_rs_cdm_t::srsran_csi_rs_cdm_nocdm;
      break;
    case asn1::rrc_nr_r17::csi_rs_res_map_s::cdm_type_opts::options::fd_cdm2:
      zp_csi_rs_resource.resource_mapping.cdm = srsran_csi_rs_cdm_t::srsran_csi_rs_cdm_fd_cdm2;
      break;
    case asn1::rrc_nr_r17::csi_rs_res_map_s::cdm_type_opts::options::cdm4_fd2_td2:
      zp_csi_rs_resource.resource_mapping.cdm = srsran_csi_rs_cdm_t::srsran_csi_rs_cdm_cdm4_fd2_td2;
      break;
    case asn1::rrc_nr_r17::csi_rs_res_map_s::cdm_type_opts::options::cdm8_fd2_td4:
      zp_csi_rs_resource.resource_mapping.cdm = srsran_csi_rs_cdm_t::srsran_csi_rs_cdm_cdm8_fd2_td4;
      break;
    default:
      asn1::log_warning("Invalid option for cdm_type %s", zp_csi_rs_res.res_map.cdm_type.to_string());
      return false;
  }

  switch (zp_csi_rs_res.res_map.density.type()) {
    case asn1::rrc_nr_r17::csi_rs_res_map_s::density_c_::types_opts::options::dot5:
      switch (zp_csi_rs_res.res_map.density.dot5()) {
        case asn1::rrc_nr_r17::csi_rs_res_map_s::density_c_::dot5_opts::options::even_prbs:
          zp_csi_rs_resource.resource_mapping.density = srsran_csi_rs_resource_mapping_density_dot5_even;
          break;
        case asn1::rrc_nr_r17::csi_rs_res_map_s::density_c_::dot5_opts::options::odd_prbs:
          zp_csi_rs_resource.resource_mapping.density = srsran_csi_rs_resource_mapping_density_dot5_odd;
          break;
        default:
          asn1::log_warning("Invalid option for dot5 %s", zp_csi_rs_res.res_map.density.dot5().to_string());
          return false;
      }
      break;
    case asn1::rrc_nr_r17::csi_rs_res_map_s::density_c_::types_opts::options::one:
      zp_csi_rs_resource.resource_mapping.density = srsran_csi_rs_resource_mapping_density_one;
      break;
    case asn1::rrc_nr_r17::csi_rs_res_map_s::density_c_::types_opts::options::three:
      zp_csi_rs_resource.resource_mapping.density = srsran_csi_rs_resource_mapping_density_three;
      break;
    case asn1::rrc_nr_r17::csi_rs_res_map_s::density_c_::types_opts::options::spare:
      zp_csi_rs_resource.resource_mapping.density = srsran_csi_rs_resource_mapping_density_spare;
      break;
    default:
      asn1::log_warning("Invalid option for density %s", zp_csi_rs_res.res_map.density.type().to_string());
      return false;
  }
  zp_csi_rs_resource.resource_mapping.freq_band.nof_rb   = zp_csi_rs_res.res_map.freq_band.nrof_rbs;
  zp_csi_rs_resource.resource_mapping.freq_band.start_rb = zp_csi_rs_res.res_map.freq_band.start_rb;

  // Validate CSI-RS resource mapping
  if (not srsran_csi_rs_resource_mapping_is_valid(&zp_csi_rs_resource.resource_mapping)) {
    asn1::json_writer json_writer;
    zp_csi_rs_res.res_map.to_json(json_writer);
    asn1::log_error("Resource mapping is invalid or not implemented: %s", json_writer.to_string());
    return false;
  }

  if (zp_csi_rs_res.periodicity_and_offset_present) {
    switch (zp_csi_rs_res.periodicity_and_offset.type()) {
      case asn1::rrc_nr_r17::csi_res_periodicity_and_offset_c::types_opts::options::slots4:
        zp_csi_rs_resource.periodicity.period = 4;
        zp_csi_rs_resource.periodicity.offset = zp_csi_rs_res.periodicity_and_offset.slots4();
        break;
      case asn1::rrc_nr_r17::csi_res_periodicity_and_offset_c::types_opts::options::slots5:
        zp_csi_rs_resource.periodicity.period = 5;
        zp_csi_rs_resource.periodicity.offset = zp_csi_rs_res.periodicity_and_offset.slots5();
        break;
      case asn1::rrc_nr_r17::csi_res_periodicity_and_offset_c::types_opts::options::slots8:
        zp_csi_rs_resource.periodicity.period = 8;
        zp_csi_rs_resource.periodicity.offset = zp_csi_rs_res.periodicity_and_offset.slots8();
        break;
      case asn1::rrc_nr_r17::csi_res_periodicity_and_offset_c::types_opts::options::slots10:
        zp_csi_rs_resource.periodicity.period = 10;
        zp_csi_rs_resource.periodicity.offset = zp_csi_rs_res.periodicity_and_offset.slots10();
        break;
      case asn1::rrc_nr_r17::csi_res_periodicity_and_offset_c::types_opts::options::slots16:
        zp_csi_rs_resource.periodicity.period = 16;
        zp_csi_rs_resource.periodicity.offset = zp_csi_rs_res.periodicity_and_offset.slots16();
        break;
      case asn1::rrc_nr_r17::csi_res_periodicity_and_offset_c::types_opts::options::slots20:
        zp_csi_rs_resource.periodicity.period = 20;
        zp_csi_rs_resource.periodicity.offset = zp_csi_rs_res.periodicity_and_offset.slots20();
        break;
      case asn1::rrc_nr_r17::csi_res_periodicity_and_offset_c::types_opts::options::slots32:
        zp_csi_rs_resource.periodicity.period = 32;
        zp_csi_rs_resource.periodicity.offset = zp_csi_rs_res.periodicity_and_offset.slots32();
        break;
      case asn1::rrc_nr_r17::csi_res_periodicity_and_offset_c::types_opts::options::slots40:
        zp_csi_rs_resource.periodicity.period = 40;
        zp_csi_rs_resource.periodicity.offset = zp_csi_rs_res.periodicity_and_offset.slots40();
        break;
      case asn1::rrc_nr_r17::csi_res_periodicity_and_offset_c::types_opts::options::slots64:
        zp_csi_rs_resource.periodicity.period = 64;
        zp_csi_rs_resource.periodicity.offset = zp_csi_rs_res.periodicity_and_offset.slots64();
        break;
      case asn1::rrc_nr_r17::csi_res_periodicity_and_offset_c::types_opts::options::slots80:
        zp_csi_rs_resource.periodicity.period = 80;
        zp_csi_rs_resource.periodicity.offset = zp_csi_rs_res.periodicity_and_offset.slots80();
        break;
      case asn1::rrc_nr_r17::csi_res_periodicity_and_offset_c::types_opts::options::slots160:
        zp_csi_rs_resource.periodicity.period = 160;
        zp_csi_rs_resource.periodicity.offset = zp_csi_rs_res.periodicity_and_offset.slots160();
        break;
      case asn1::rrc_nr_r17::csi_res_periodicity_and_offset_c::types_opts::options::slots320:
        zp_csi_rs_resource.periodicity.period = 320;
        zp_csi_rs_resource.periodicity.offset = zp_csi_rs_res.periodicity_and_offset.slots320();
        break;
      case asn1::rrc_nr_r17::csi_res_periodicity_and_offset_c::types_opts::options::slots640:
        zp_csi_rs_resource.periodicity.period = 640;
        zp_csi_rs_resource.periodicity.offset = zp_csi_rs_res.periodicity_and_offset.slots640();
        break;
      default:
        asn1::log_warning("Invalid option for periodicity_and_offset %s",
                          zp_csi_rs_res.periodicity_and_offset.type().to_string());
        return false;
    }
  } else {
    asn1::log_warning("Option periodicity_and_offset not present");
    return false;
  }

  *out_zp_csi_rs_resource = zp_csi_rs_resource;
  return true;
}

bool make_phy_nzp_csi_rs_resource(const asn1::rrc_nr_r17::nzp_csi_rs_res_s& asn1_nzp_csi_rs_res,
                                  srsran_csi_rs_nzp_resource_t*             out_csi_rs_nzp_resource)
{
  srsran_csi_rs_nzp_resource_t csi_rs_nzp_resource = {};
  csi_rs_nzp_resource.id                           = asn1_nzp_csi_rs_res.nzp_csi_rs_res_id;
  switch (asn1_nzp_csi_rs_res.res_map.freq_domain_alloc.type()) {
    case asn1::rrc_nr_r17::csi_rs_res_map_s::freq_domain_alloc_c_::types_opts::options::row1:
      csi_rs_nzp_resource.resource_mapping.row = srsran_csi_rs_resource_mapping_row_1;
      for (uint32_t i = 0; i < asn1_nzp_csi_rs_res.res_map.freq_domain_alloc.row1().length(); i++) {
        csi_rs_nzp_resource.resource_mapping.frequency_domain_alloc[i] =
            asn1_nzp_csi_rs_res.res_map.freq_domain_alloc.row1().get(
                asn1_nzp_csi_rs_res.res_map.freq_domain_alloc.row1().length() - 1 - i);
      }
      break;
    case asn1::rrc_nr_r17::csi_rs_res_map_s::freq_domain_alloc_c_::types_opts::options::row2:
      csi_rs_nzp_resource.resource_mapping.row = srsran_csi_rs_resource_mapping_row_2;
      for (uint32_t i = 0; i < asn1_nzp_csi_rs_res.res_map.freq_domain_alloc.row2().length(); i++) {
        csi_rs_nzp_resource.resource_mapping.frequency_domain_alloc[i] =
            asn1_nzp_csi_rs_res.res_map.freq_domain_alloc.row2().get(
                asn1_nzp_csi_rs_res.res_map.freq_domain_alloc.row2().length() - 1 - i);
      }
      break;
    case asn1::rrc_nr_r17::csi_rs_res_map_s::freq_domain_alloc_c_::types_opts::options::row4:
      csi_rs_nzp_resource.resource_mapping.row = srsran_csi_rs_resource_mapping_row_4;
      for (uint32_t i = 0; i < asn1_nzp_csi_rs_res.res_map.freq_domain_alloc.row4().length(); i++) {
        csi_rs_nzp_resource.resource_mapping.frequency_domain_alloc[i] =
            asn1_nzp_csi_rs_res.res_map.freq_domain_alloc.row4().get(
                asn1_nzp_csi_rs_res.res_map.freq_domain_alloc.row4().length() - 1 - i);
      }
      break;
    case asn1::rrc_nr_r17::csi_rs_res_map_s::freq_domain_alloc_c_::types_opts::options::other:
      csi_rs_nzp_resource.resource_mapping.row = srsran_csi_rs_resource_mapping_row_other;
      for (uint32_t i = 0; i < asn1_nzp_csi_rs_res.res_map.freq_domain_alloc.other().length(); i++) {
        csi_rs_nzp_resource.resource_mapping.frequency_domain_alloc[i] =
            asn1_nzp_csi_rs_res.res_map.freq_domain_alloc.other().get(
                asn1_nzp_csi_rs_res.res_map.freq_domain_alloc.other().length() - 1 - i);
      }
      break;
    default:
      asn1::log_warning("Invalid option for freq_domain_alloc %s",
                        asn1_nzp_csi_rs_res.res_map.freq_domain_alloc.type().to_string());
      return false;
  }

  csi_rs_nzp_resource.resource_mapping.nof_ports        = asn1_nzp_csi_rs_res.res_map.nrof_ports.to_number();
  csi_rs_nzp_resource.resource_mapping.first_symbol_idx = asn1_nzp_csi_rs_res.res_map.first_ofdm_symbol_in_time_domain;
  if (asn1_nzp_csi_rs_res.res_map.first_ofdm_symbol_in_time_domain2_present) {
    csi_rs_nzp_resource.resource_mapping.first_symbol_idx2 =
        asn1_nzp_csi_rs_res.res_map.first_ofdm_symbol_in_time_domain2;
  }

  switch (asn1_nzp_csi_rs_res.res_map.cdm_type) {
    case asn1::rrc_nr_r17::csi_rs_res_map_s::cdm_type_opts::options::no_cdm:
      csi_rs_nzp_resource.resource_mapping.cdm = srsran_csi_rs_cdm_t::srsran_csi_rs_cdm_nocdm;
      break;
    case asn1::rrc_nr_r17::csi_rs_res_map_s::cdm_type_opts::options::fd_cdm2:
      csi_rs_nzp_resource.resource_mapping.cdm = srsran_csi_rs_cdm_t::srsran_csi_rs_cdm_fd_cdm2;
      break;
    case asn1::rrc_nr_r17::csi_rs_res_map_s::cdm_type_opts::options::cdm4_fd2_td2:
      csi_rs_nzp_resource.resource_mapping.cdm = srsran_csi_rs_cdm_t::srsran_csi_rs_cdm_cdm4_fd2_td2;
      break;
    case asn1::rrc_nr_r17::csi_rs_res_map_s::cdm_type_opts::options::cdm8_fd2_td4:
      csi_rs_nzp_resource.resource_mapping.cdm = srsran_csi_rs_cdm_t::srsran_csi_rs_cdm_cdm8_fd2_td4;
      break;
    default:
      asn1::log_warning("Invalid option for cdm_type %s", asn1_nzp_csi_rs_res.res_map.cdm_type.to_string());
      return false;
  }

  switch (asn1_nzp_csi_rs_res.res_map.density.type()) {
    case asn1::rrc_nr_r17::csi_rs_res_map_s::density_c_::types_opts::options::dot5:
      switch (asn1_nzp_csi_rs_res.res_map.density.dot5()) {
        case asn1::rrc_nr_r17::csi_rs_res_map_s::density_c_::dot5_opts::options::even_prbs:
          csi_rs_nzp_resource.resource_mapping.density = srsran_csi_rs_resource_mapping_density_dot5_even;
          break;
        case asn1::rrc_nr_r17::csi_rs_res_map_s::density_c_::dot5_opts::options::odd_prbs:
          csi_rs_nzp_resource.resource_mapping.density = srsran_csi_rs_resource_mapping_density_dot5_odd;
          break;
        default:
          asn1::log_warning("Invalid option for dot5 %s", asn1_nzp_csi_rs_res.res_map.density.dot5().to_string());
          return false;
      }
      break;
    case asn1::rrc_nr_r17::csi_rs_res_map_s::density_c_::types_opts::options::one:
      csi_rs_nzp_resource.resource_mapping.density = srsran_csi_rs_resource_mapping_density_one;
      break;
    case asn1::rrc_nr_r17::csi_rs_res_map_s::density_c_::types_opts::options::three:
      csi_rs_nzp_resource.resource_mapping.density = srsran_csi_rs_resource_mapping_density_three;
      break;
    case asn1::rrc_nr_r17::csi_rs_res_map_s::density_c_::types_opts::options::spare:
      csi_rs_nzp_resource.resource_mapping.density = srsran_csi_rs_resource_mapping_density_spare;
      break;
    default:
      asn1::log_warning("Invalid option for density %s", asn1_nzp_csi_rs_res.res_map.density.type().to_string());
      return false;
  }
  csi_rs_nzp_resource.resource_mapping.freq_band.nof_rb   = asn1_nzp_csi_rs_res.res_map.freq_band.nrof_rbs;
  csi_rs_nzp_resource.resource_mapping.freq_band.start_rb = asn1_nzp_csi_rs_res.res_map.freq_band.start_rb;

  // Validate CSI-RS resource mapping
  if (not srsran_csi_rs_resource_mapping_is_valid(&csi_rs_nzp_resource.resource_mapping)) {
    asn1::json_writer json_writer;
    asn1_nzp_csi_rs_res.res_map.to_json(json_writer);
    asn1::log_error("Resource mapping is invalid or not implemented: %s", json_writer.to_string());
    return false;
  }

  csi_rs_nzp_resource.power_control_offset = asn1_nzp_csi_rs_res.pwr_ctrl_offset;
  if (asn1_nzp_csi_rs_res.pwr_ctrl_offset_ss_present) {
    csi_rs_nzp_resource.power_control_offset_ss = asn1_nzp_csi_rs_res.pwr_ctrl_offset_ss.to_number();
  }

  if (asn1_nzp_csi_rs_res.periodicity_and_offset_present) {
    switch (asn1_nzp_csi_rs_res.periodicity_and_offset.type()) {
      case asn1::rrc_nr_r17::csi_res_periodicity_and_offset_c::types_opts::options::slots4:
        csi_rs_nzp_resource.periodicity.period = 4;
        csi_rs_nzp_resource.periodicity.offset = asn1_nzp_csi_rs_res.periodicity_and_offset.slots4();
        break;
      case asn1::rrc_nr_r17::csi_res_periodicity_and_offset_c::types_opts::options::slots5:
        csi_rs_nzp_resource.periodicity.period = 5;
        csi_rs_nzp_resource.periodicity.offset = asn1_nzp_csi_rs_res.periodicity_and_offset.slots5();
        break;
      case asn1::rrc_nr_r17::csi_res_periodicity_and_offset_c::types_opts::options::slots8:
        csi_rs_nzp_resource.periodicity.period = 8;
        csi_rs_nzp_resource.periodicity.offset = asn1_nzp_csi_rs_res.periodicity_and_offset.slots8();
        break;
      case asn1::rrc_nr_r17::csi_res_periodicity_and_offset_c::types_opts::options::slots10:
        csi_rs_nzp_resource.periodicity.period = 10;
        csi_rs_nzp_resource.periodicity.offset = asn1_nzp_csi_rs_res.periodicity_and_offset.slots10();
        break;
      case asn1::rrc_nr_r17::csi_res_periodicity_and_offset_c::types_opts::options::slots16:
        csi_rs_nzp_resource.periodicity.period = 16;
        csi_rs_nzp_resource.periodicity.offset = asn1_nzp_csi_rs_res.periodicity_and_offset.slots16();
        break;
      case asn1::rrc_nr_r17::csi_res_periodicity_and_offset_c::types_opts::options::slots20:
        csi_rs_nzp_resource.periodicity.period = 20;
        csi_rs_nzp_resource.periodicity.offset = asn1_nzp_csi_rs_res.periodicity_and_offset.slots20();
        break;
      case asn1::rrc_nr_r17::csi_res_periodicity_and_offset_c::types_opts::options::slots32:
        csi_rs_nzp_resource.periodicity.period = 32;
        csi_rs_nzp_resource.periodicity.offset = asn1_nzp_csi_rs_res.periodicity_and_offset.slots32();
        break;
      case asn1::rrc_nr_r17::csi_res_periodicity_and_offset_c::types_opts::options::slots40:
        csi_rs_nzp_resource.periodicity.period = 40;
        csi_rs_nzp_resource.periodicity.offset = asn1_nzp_csi_rs_res.periodicity_and_offset.slots40();
        break;
      case asn1::rrc_nr_r17::csi_res_periodicity_and_offset_c::types_opts::options::slots64:
        csi_rs_nzp_resource.periodicity.period = 64;
        csi_rs_nzp_resource.periodicity.offset = asn1_nzp_csi_rs_res.periodicity_and_offset.slots64();
        break;
      case asn1::rrc_nr_r17::csi_res_periodicity_and_offset_c::types_opts::options::slots80:
        csi_rs_nzp_resource.periodicity.period = 80;
        csi_rs_nzp_resource.periodicity.offset = asn1_nzp_csi_rs_res.periodicity_and_offset.slots80();
        break;
      case asn1::rrc_nr_r17::csi_res_periodicity_and_offset_c::types_opts::options::slots160:
        csi_rs_nzp_resource.periodicity.period = 160;
        csi_rs_nzp_resource.periodicity.offset = asn1_nzp_csi_rs_res.periodicity_and_offset.slots160();
        break;
      case asn1::rrc_nr_r17::csi_res_periodicity_and_offset_c::types_opts::options::slots320:
        csi_rs_nzp_resource.periodicity.period = 320;
        csi_rs_nzp_resource.periodicity.offset = asn1_nzp_csi_rs_res.periodicity_and_offset.slots320();
        break;
      case asn1::rrc_nr_r17::csi_res_periodicity_and_offset_c::types_opts::options::slots640:
        csi_rs_nzp_resource.periodicity.period = 640;
        csi_rs_nzp_resource.periodicity.offset = asn1_nzp_csi_rs_res.periodicity_and_offset.slots640();
        break;
      default:
        asn1::log_warning("Invalid option for periodicity_and_offset %s",
                          asn1_nzp_csi_rs_res.periodicity_and_offset.type().to_string());
        return false;
    }
  } else {
    asn1::log_warning("Option periodicity_and_offset not present");
    return false;
  }

  csi_rs_nzp_resource.scrambling_id = asn1_nzp_csi_rs_res.scrambling_id;

  *out_csi_rs_nzp_resource = csi_rs_nzp_resource;
  return true;
}

static bool make_phy_max_code_rate(const asn1::rrc_nr_r17::pucch_format_cfg_s& pucch_format_cfg,
                                   uint32_t*                                   in_max_code_rate)
{
  if (not pucch_format_cfg.max_code_rate_present) {
    return false;
  }
  *in_max_code_rate = pucch_format_cfg.max_code_rate.value;
  return true;
}

bool make_phy_res_config(const asn1::rrc_nr_r17::pucch_res_s& pucch_res,
                         uint32_t                             format_2_max_code_rate,
                         srsran_pucch_nr_resource_t*          in_srsran_pucch_nr_resource)
{
  srsran_pucch_nr_resource_t srsran_pucch_nr_resource = {};
  srsran_pucch_nr_resource.starting_prb               = pucch_res.start_prb;
  srsran_pucch_nr_resource.intra_slot_hopping         = pucch_res.intra_slot_freq_hop_present;
  if (pucch_res.second_hop_prb_present) {
    srsran_pucch_nr_resource.second_hop_prb = pucch_res.second_hop_prb;
  }
  switch (pucch_res.format.type()) {
    case asn1::rrc_nr_r17::pucch_res_s::format_c_::types_opts::format0:
      srsran_pucch_nr_resource.format = SRSRAN_PUCCH_NR_FORMAT_0;
      break;
    case asn1::rrc_nr_r17::pucch_res_s::format_c_::types_opts::format1:
      srsran_pucch_nr_resource.format               = SRSRAN_PUCCH_NR_FORMAT_1;
      srsran_pucch_nr_resource.initial_cyclic_shift = pucch_res.format.format1().init_cyclic_shift;
      srsran_pucch_nr_resource.nof_symbols          = pucch_res.format.format1().nrof_symbols;
      srsran_pucch_nr_resource.start_symbol_idx     = pucch_res.format.format1().start_symbol_idx;
      srsran_pucch_nr_resource.time_domain_occ      = pucch_res.format.format1().time_domain_occ;
      break;
    case asn1::rrc_nr_r17::pucch_res_s::format_c_::types_opts::format2:
      srsran_pucch_nr_resource.format           = SRSRAN_PUCCH_NR_FORMAT_2;
      srsran_pucch_nr_resource.nof_symbols      = pucch_res.format.format2().nrof_symbols;
      srsran_pucch_nr_resource.start_symbol_idx = pucch_res.format.format2().start_symbol_idx;
      srsran_pucch_nr_resource.nof_prb          = pucch_res.format.format2().nrof_prbs;
      break;
    case asn1::rrc_nr_r17::pucch_res_s::format_c_::types_opts::format3:
      srsran_pucch_nr_resource.format           = SRSRAN_PUCCH_NR_FORMAT_3;
      srsran_pucch_nr_resource.nof_symbols      = pucch_res.format.format3().nrof_symbols;
      srsran_pucch_nr_resource.start_symbol_idx = pucch_res.format.format3().start_symbol_idx;
      srsran_pucch_nr_resource.nof_prb          = pucch_res.format.format3().nrof_prbs;
      break;
    case asn1::rrc_nr_r17::pucch_res_s::format_c_::types_opts::format4:
      srsran_pucch_nr_resource.format = SRSRAN_PUCCH_NR_FORMAT_4;
      asn1::log_warning("SRSRAN_PUCCH_NR_FORMAT_4 conversion not supported");
      return false;
    default:
      srsran_pucch_nr_resource.format = SRSRAN_PUCCH_NR_FORMAT_ERROR;
      return false;
  }
  srsran_pucch_nr_resource.max_code_rate = format_2_max_code_rate;
  *in_srsran_pucch_nr_resource           = srsran_pucch_nr_resource;
  return true;
}

/* Apply pucch_cfg in BWP-UplinkDedicated */
bool apply_dedicated_pucch_cfg(srsran::phy_cfg_nr_t&                phy_cfg,
                               const asn1::rrc_nr_r17::pucch_cfg_s& dedicated_pucch_cfg,
                               srsran::static_circular_map<uint32_t, srsran_pucch_nr_resource_t, 128UL> pucch_res_list)
{
  bool success = true;
  // determine format and max code rate
  uint32_t max_code_rate = 0;
  if (dedicated_pucch_cfg.format1_present && dedicated_pucch_cfg.format1.setup().max_code_rate_present) {
    if (make_phy_max_code_rate(dedicated_pucch_cfg.format1.setup(), &max_code_rate) == false) {
      success = false;
    }
  } else if (dedicated_pucch_cfg.format2_present && dedicated_pucch_cfg.format2.setup().max_code_rate_present) {
    if (make_phy_max_code_rate(dedicated_pucch_cfg.format2.setup(), &max_code_rate) == false) {
      success = false;
    }
  } else if (dedicated_pucch_cfg.format3_present && dedicated_pucch_cfg.format3.setup().max_code_rate_present) {
    if (make_phy_max_code_rate(dedicated_pucch_cfg.format3.setup(), &max_code_rate) == false) {
      success = false;
    }
  } else {
    asn1::log_warning("Option format1/2/3 not present for pucch config, using default max code rate 0");
  }

  // now look up resource and assign into internal struct
  if (dedicated_pucch_cfg.res_to_add_mod_list.size() > 0) {
    for (uint32_t i = 0; i < dedicated_pucch_cfg.res_to_add_mod_list.size(); i++) {
      uint32_t res_id = dedicated_pucch_cfg.res_to_add_mod_list[i].pucch_res_id;
      pucch_res_list.insert(res_id, {});
      if (!make_phy_res_config(dedicated_pucch_cfg.res_to_add_mod_list[i], max_code_rate, &pucch_res_list[res_id])) {
        success = false;
      }
    }
  }

  // resourceToAddModList
  phy_cfg.pucch.enabled = true;
  if (dedicated_pucch_cfg.res_set_to_add_mod_list.size() > 0) {
    for (uint32_t i = 0; i < dedicated_pucch_cfg.res_set_to_add_mod_list.size(); i++) {
      uint32_t set_id                          = dedicated_pucch_cfg.res_set_to_add_mod_list[i].pucch_res_set_id;
      phy_cfg.pucch.sets[set_id].nof_resources = dedicated_pucch_cfg.res_set_to_add_mod_list[i].res_list.size();
      for (uint32_t j = 0; j < dedicated_pucch_cfg.res_set_to_add_mod_list[i].res_list.size(); j++) {
        uint32_t res_id = dedicated_pucch_cfg.res_set_to_add_mod_list[i].res_list[j];
        if (pucch_res_list.contains(res_id)) {
          phy_cfg.pucch.sets[set_id].resources[j] = pucch_res_list[res_id];
        } else {
          asn1::log_error(
              "Resources set not present for assign pucch sets (res_id %d, setid %d, j %d)", res_id, set_id, j);
        }
      }
    }
  }

  // schedulingRequestResourceToAddModList
  if (dedicated_pucch_cfg.sched_request_res_to_add_mod_list.size() > 0) {
    for (uint32_t i = 0; i < dedicated_pucch_cfg.sched_request_res_to_add_mod_list.size(); i++) {
      uint32_t sr_res_id = dedicated_pucch_cfg.sched_request_res_to_add_mod_list[i].sched_request_res_id;
      srsran_pucch_nr_sr_resource_t srsran_pucch_nr_sr_resource;
      if (make_phy_sr_resource(dedicated_pucch_cfg.sched_request_res_to_add_mod_list[i],
                               &srsran_pucch_nr_sr_resource) == true) { // TODO: fix that if indexing is solved
        phy_cfg.pucch.sr_resources[sr_res_id] = srsran_pucch_nr_sr_resource;
        // Set PUCCH resource
        if (dedicated_pucch_cfg.sched_request_res_to_add_mod_list[i].res_present) {
          uint32_t pucch_res_id = dedicated_pucch_cfg.sched_request_res_to_add_mod_list[i].res;
          if (pucch_res_list.contains(pucch_res_id)) {
            phy_cfg.pucch.sr_resources[sr_res_id].resource = pucch_res_list[pucch_res_id];
          } else {
            asn1::log_warning("Warning SR (%d) PUCCH resource is invalid (%d)", sr_res_id, pucch_res_id);
            phy_cfg.pucch.sr_resources[sr_res_id].configured = false;
            return false;
          }
        }
      }
    }
  }

  if (dedicated_pucch_cfg.dl_data_to_ul_ack.size() > 0) {
    for (uint32_t i = 0; i < dedicated_pucch_cfg.dl_data_to_ul_ack.size(); i++) {
      phy_cfg.harq_ack.dl_data_to_ul_ack[i] = dedicated_pucch_cfg.dl_data_to_ul_ack[i];
    }
    phy_cfg.harq_ack.nof_dl_data_to_ul_ack = dedicated_pucch_cfg.dl_data_to_ul_ack.size();
  }

  return true;
};

bool make_phy_sr_resource(const asn1::rrc_nr_r17::sched_request_res_cfg_s& sched_request_res_cfg,
                          srsran_pucch_nr_sr_resource_t*                   in_srsran_pucch_nr_sr_resource)
{
  srsran_pucch_nr_sr_resource_t srsran_pucch_nr_sr_resource = {};
  srsran_pucch_nr_sr_resource.sr_id                         = sched_request_res_cfg.sched_request_id;
  if (sched_request_res_cfg.periodicity_and_offset_present && sched_request_res_cfg.res_present) {
    srsran_pucch_nr_sr_resource.configured = true;
    switch (sched_request_res_cfg.periodicity_and_offset.type()) {
      case asn1::rrc_nr_r17::sched_request_res_cfg_s::periodicity_and_offset_c_::types_opts::sl2:
        srsran_pucch_nr_sr_resource.period = 2;
        srsran_pucch_nr_sr_resource.offset = sched_request_res_cfg.periodicity_and_offset.sl2();
        break;
      case asn1::rrc_nr_r17::sched_request_res_cfg_s::periodicity_and_offset_c_::types_opts::sl4:
        srsran_pucch_nr_sr_resource.period = 4;
        srsran_pucch_nr_sr_resource.offset = sched_request_res_cfg.periodicity_and_offset.sl4();
        break;
      case asn1::rrc_nr_r17::sched_request_res_cfg_s::periodicity_and_offset_c_::types_opts::sl5:
        srsran_pucch_nr_sr_resource.period = 5;
        srsran_pucch_nr_sr_resource.offset = sched_request_res_cfg.periodicity_and_offset.sl5();
        break;
      case asn1::rrc_nr_r17::sched_request_res_cfg_s::periodicity_and_offset_c_::types_opts::sl8:
        srsran_pucch_nr_sr_resource.period = 8;
        srsran_pucch_nr_sr_resource.offset = sched_request_res_cfg.periodicity_and_offset.sl8();
        break;
      case asn1::rrc_nr_r17::sched_request_res_cfg_s::periodicity_and_offset_c_::types_opts::sl10:
        srsran_pucch_nr_sr_resource.period = 10;
        srsran_pucch_nr_sr_resource.offset = sched_request_res_cfg.periodicity_and_offset.sl10();
        break;
      case asn1::rrc_nr_r17::sched_request_res_cfg_s::periodicity_and_offset_c_::types_opts::sl16:
        srsran_pucch_nr_sr_resource.period = 16;
        srsran_pucch_nr_sr_resource.offset = sched_request_res_cfg.periodicity_and_offset.sl16();
        break;
      case asn1::rrc_nr_r17::sched_request_res_cfg_s::periodicity_and_offset_c_::types_opts::sl20:
        srsran_pucch_nr_sr_resource.period = 20;
        srsran_pucch_nr_sr_resource.offset = sched_request_res_cfg.periodicity_and_offset.sl20();
        break;
      case asn1::rrc_nr_r17::sched_request_res_cfg_s::periodicity_and_offset_c_::types_opts::sl40:
        srsran_pucch_nr_sr_resource.period = 40;
        srsran_pucch_nr_sr_resource.offset = sched_request_res_cfg.periodicity_and_offset.sl40();
        break;
      case asn1::rrc_nr_r17::sched_request_res_cfg_s::periodicity_and_offset_c_::types_opts::sl80:
        srsran_pucch_nr_sr_resource.period = 80;
        srsran_pucch_nr_sr_resource.offset = sched_request_res_cfg.periodicity_and_offset.sl80();
        break;
      case asn1::rrc_nr_r17::sched_request_res_cfg_s::periodicity_and_offset_c_::types_opts::sl160:
        srsran_pucch_nr_sr_resource.period = 160;
        srsran_pucch_nr_sr_resource.offset = sched_request_res_cfg.periodicity_and_offset.sl160();
        break;
      case asn1::rrc_nr_r17::sched_request_res_cfg_s::periodicity_and_offset_c_::types_opts::sl320:
        srsran_pucch_nr_sr_resource.period = 320;
        srsran_pucch_nr_sr_resource.offset = sched_request_res_cfg.periodicity_and_offset.sl320();
        break;
      case asn1::rrc_nr_r17::sched_request_res_cfg_s::periodicity_and_offset_c_::types_opts::sl640:
        srsran_pucch_nr_sr_resource.period = 640;
        srsran_pucch_nr_sr_resource.offset = sched_request_res_cfg.periodicity_and_offset.sl640();
        break;
      case asn1::rrc_nr_r17::sched_request_res_cfg_s::periodicity_and_offset_c_::types_opts::sym2:
      case asn1::rrc_nr_r17::sched_request_res_cfg_s::periodicity_and_offset_c_::types_opts::sym6or7:
      case asn1::rrc_nr_r17::sched_request_res_cfg_s::periodicity_and_offset_c_::types_opts::sl1:
      default:
        srsran_pucch_nr_sr_resource.configured = false;
        asn1::log_warning("Invalid option for periodicity_and_offset %s",
                          sched_request_res_cfg.periodicity_and_offset.type().to_string());
        return false;
    }

  } else {
    srsran_pucch_nr_sr_resource.configured = false;
  }
  *in_srsran_pucch_nr_sr_resource = srsran_pucch_nr_sr_resource;
  return true;
}

bool make_phy_pusch_alloc_type(const asn1::rrc_nr_r17::pusch_cfg_s& pusch_cfg,
                               srsran_resource_alloc_t*             in_srsran_resource_alloc)
{
  srsran_resource_alloc_t srsran_resource_alloc = {};

  switch (pusch_cfg.res_alloc) {
    case asn1::rrc_nr_r17::pusch_cfg_s::res_alloc_e_::res_alloc_type0:
      srsran_resource_alloc = srsran_resource_alloc_type0;
      break;
    case asn1::rrc_nr_r17::pusch_cfg_s::res_alloc_e_::res_alloc_type1:
      srsran_resource_alloc = srsran_resource_alloc_type1;
      break;
    case asn1::rrc_nr_r17::pusch_cfg_s::res_alloc_e_::dyn_switch:
      srsran_resource_alloc = srsran_resource_alloc_dynamic;
      break;
    default:
      asn1::log_warning("Invalid option for pusch::resource_alloc %s", pusch_cfg.res_alloc.to_string());
      return false;
  }
  *in_srsran_resource_alloc = srsran_resource_alloc;
  return true;
}

bool make_phy_beta_offsets(const asn1::rrc_nr_r17::beta_offsets_s& beta_offsets,
                           srsran_beta_offsets_t*                  in_srsran_beta_offsets)
{
  srsran_beta_offsets_t srsran_beta_offsets = {};

  srsran_beta_offsets.ack_index1 = beta_offsets.beta_offset_ack_idx1_present ? beta_offsets.beta_offset_ack_idx1 : 11;
  srsran_beta_offsets.ack_index2 = beta_offsets.beta_offset_ack_idx2_present ? beta_offsets.beta_offset_ack_idx2 : 11;
  srsran_beta_offsets.ack_index3 = beta_offsets.beta_offset_ack_idx3_present ? beta_offsets.beta_offset_ack_idx3 : 11;
  srsran_beta_offsets.csi1_index1 =
      beta_offsets.beta_offset_csi_part1_idx1_present ? beta_offsets.beta_offset_csi_part1_idx1 : 13;
  srsran_beta_offsets.csi1_index2 =
      beta_offsets.beta_offset_csi_part1_idx2_present ? beta_offsets.beta_offset_csi_part1_idx2 : 13;
  srsran_beta_offsets.csi2_index1 =
      beta_offsets.beta_offset_csi_part2_idx1_present ? beta_offsets.beta_offset_csi_part2_idx1 : 13;
  srsran_beta_offsets.csi2_index2 =
      beta_offsets.beta_offset_csi_part2_idx2_present ? beta_offsets.beta_offset_csi_part2_idx2 : 13;
  *in_srsran_beta_offsets = srsran_beta_offsets;
  return true;
}

bool make_phy_pusch_scaling(const asn1::rrc_nr_r17::uci_on_pusch_s& uci_on_pusch, float* in_scaling)
{
  float pusch_scaling = 0;
  switch (uci_on_pusch.scaling) {
    case asn1::rrc_nr_r17::uci_on_pusch_s::scaling_opts::f0p5:
      pusch_scaling = 0.5;
      break;
    case asn1::rrc_nr_r17::uci_on_pusch_s::scaling_opts::f0p65:
      pusch_scaling = 0.65;
      break;
    case asn1::rrc_nr_r17::uci_on_pusch_s::scaling_opts::f0p8:
      pusch_scaling = 0.8;
      break;
    case asn1::rrc_nr_r17::uci_on_pusch_s::scaling_opts::f1:
      pusch_scaling = 1.0;
      break;
    default:
      asn1::log_warning("Invalid option for scaling %s", uci_on_pusch.scaling.to_string());
      return false;
  }
  *in_scaling = pusch_scaling;
  return true;
}
