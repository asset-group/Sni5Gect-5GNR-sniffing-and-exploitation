#include "shadower/utils/phy_cfg_utils.h"
#include "shadower/utils/rrc_nr_utils_r17.h"
#include "srsran/asn1/rrc_nr.h"
#include "srsran/asn1/rrc_nr/bcch_dl_sch_msg.h"
#include "srsran/asn1/rrc_nr_utils.h"

/* Initialize phy cfg from shadower configuration */
void init_phy_cfg(srsran::phy_cfg_nr_t& phy_cfg, ShadowerConfig& config)
{
  phy_cfg.carrier.dl_center_frequency_hz = config.dl_freq;
  phy_cfg.carrier.ul_center_frequency_hz = config.ul_freq;
  phy_cfg.carrier.ssb_center_freq_hz     = config.ssb_freq;
  phy_cfg.carrier.sample_rate_hz         = config.sample_rate;
  phy_cfg.carrier.offset_to_carrier      = config.offset_to_carrier;
  phy_cfg.carrier.scs                    = config.scs_common;
  phy_cfg.carrier.nof_prb                = config.nof_prb;
  phy_cfg.carrier.max_mimo_layers        = 1;
  phy_cfg.duplex.mode                    = config.duplex_mode;
  phy_cfg.ssb.periodicity_ms             = config.ssb_period_ms;
  phy_cfg.ssb.position_in_burst[0]       = true;
  phy_cfg.ssb.scs                        = config.scs_ssb;
  phy_cfg.ssb.pattern                    = config.ssb_pattern;
}

/* Initialize phy state object */
void init_phy_state(srsue::nr::state& phy_state, uint32_t nof_prb)
{
  /* physical state to help track grants */
  phy_state.stack                 = nullptr;
  phy_state.args.nof_carriers     = 1;
  phy_state.args.dl.nof_max_prb   = nof_prb;
  phy_state.args.dl.pdsch.max_prb = nof_prb;
  phy_state.args.ul.nof_max_prb   = nof_prb;
  phy_state.args.ul.pusch.max_prb = nof_prb;
}

/* Decode SIB1 bytes to asn1 structure */
bool parse_to_sib1(uint8_t* data, uint32_t len, asn1::rrc_nr_r17::sib1_s& sib1)
{
  asn1::rrc_nr_r17::bcch_dl_sch_msg_s dlsch_msg;
  asn1::cbit_ref                      dlsch_bref(data, len);
  asn1::SRSASN_CODE                   err = dlsch_msg.unpack(dlsch_bref);
  if (err != asn1::SRSASN_SUCCESS) {
    asn1::log_error("Error unpacking BCCH-DL-SCH message");
    return false;
  }

  if (dlsch_msg.msg.type().value != asn1::rrc_nr_r17::bcch_dl_sch_msg_type_c::types_opts::c1) {
    asn1::log_error("Unsupported BCCH-DL-SCH message class extension");
    return false;
  }

  if (dlsch_msg.msg.c1().type().value != asn1::rrc_nr_r17::bcch_dl_sch_msg_type_c::c1_c_::types_opts::sib_type1) {
    asn1::log_error("Expected sib_type1 message");
    return false;
  }
  sib1 = dlsch_msg.msg.c1().sib_type1();
  return true;
}

/* Set rar grant */
bool set_rar_grant(uint16_t                                        rnti,
                   srsran_rnti_type_t                              rnti_type,
                   uint32_t                                        slot_idx,
                   std::array<uint8_t, SRSRAN_RAR_UL_GRANT_NBITS>& grant,
                   srsran::phy_cfg_nr_t&                           phy_cfg,
                   srsue::nr::state&                               phy_state,
                   uint32_t*                                       grant_k,
                   srslog::basic_logger&                           logger)
{
  srsran_dci_msg_nr_t dci_msg = {};
  dci_msg.ctx.format          = srsran_dci_format_nr_rar; /* MAC RAR grant shall be unpacked as DCI 0_0 format */
  dci_msg.ctx.rnti_type       = rnti_type;
  dci_msg.ctx.ss_type         = srsran_search_space_type_rar; /* This indicates it is a MAC RAR */
  dci_msg.ctx.rnti            = rnti;
  dci_msg.nof_bits            = SRSRAN_RAR_UL_GRANT_NBITS;
  srsran_vec_u8_copy(dci_msg.payload, grant.data(), SRSRAN_RAR_UL_GRANT_NBITS);
  srsran_dci_ul_nr_t dci_ul = {};
  if (srsran_dci_nr_ul_unpack(NULL, &dci_msg, &dci_ul) < SRSRAN_SUCCESS) {
    logger.error("Couldn't unpack UL grant");
    return false;
  }
  if (logger.debug.enabled()) {
    std::array<char, 512> str{};
    srsran_dci_nr_t       dci = {};
    srsran_dci_ul_nr_to_str(&dci, &dci_ul, str.data(), str.size());
    logger.debug("Setting RAR Grant: %s", str.data());
  }
  srsran_slot_cfg_t slot_cfg = {};
  slot_cfg.idx               = slot_idx;

  // Extract the K value from the grant
  phy_state.set_ul_pending_grant(phy_cfg, slot_cfg, dci_ul);
  srsran_sch_cfg_nr_t pusch_cfg = {};
  if (not phy_cfg.get_pusch_cfg(slot_cfg, dci_ul, pusch_cfg)) {
    logger.error("Computing PUSCH cfg from RAR grant failed");
    return false;
  }
  *grant_k = pusch_cfg.grant.k;
  return true;
}

/* Load mib configuration from file and apply to phy cfg */
bool configure_phy_cfg_from_mib(srsran::phy_cfg_nr_t& phy_cfg, std::string& filename, uint32_t ncellid)
{
  srsran_mib_nr_t mib = {};
  if (!read_raw_config(filename, (uint8_t*)&mib, sizeof(srsran_mib_nr_t))) {
    return false;
  }
  if (!update_phy_cfg_from_mib(phy_cfg, mib, ncellid)) {
    return false;
  }
  return true;
}

/* Load SIB1 configuration from file and apply to phy cfg */
bool configure_phy_cfg_from_sib1(srsran::phy_cfg_nr_t& phy_cfg, std::string& filename, uint32_t nbits)
{
  std::vector<uint8_t> sib1_raw(nbits);
  if (!read_raw_config(filename, sib1_raw.data(), nbits)) {
    printf("Failed to read SIB1 from %s\n", filename.c_str());
    return false;
  }

  asn1::rrc_nr_r17::sib1_s sib1;
  if (!parse_to_sib1(sib1_raw.data(), nbits, sib1)) {
    printf("Failed to parse SIB1\n");
    return false;
  }
  update_phy_cfg_from_sib1(phy_cfg, sib1);
  return true;
}

bool parse_to_dl_dcch_msg(uint8_t* data, uint32_t len, asn1::rrc_nr_r17::dl_dcch_msg_s& dl_dcch_msg)
{
  asn1::cbit_ref                  dl_dcch_bref(data, len);
  asn1::rrc_nr_r17::dl_dcch_msg_s dlsch_msg;
  asn1::SRSASN_CODE               err = dlsch_msg.unpack(dl_dcch_bref);
  if (err != asn1::SRSASN_SUCCESS) {
    asn1::log_error("Error unpacking DL-DCCH message");
    return false;
  }
  dl_dcch_msg = dlsch_msg;
  return true;
}

bool configure_phy_cfg_from_cell_group_cfg(srsran::phy_cfg_nr_t& phy_cfg, std::string& cell_group_cfg)
{
  /* Convert hex cell_group_cfg to bytes */
  std::vector<uint8_t> cell_group_cfg_bytes(cell_group_cfg.size() / 2);
  for (size_t i = 0; i < cell_group_cfg.size(); i += 2) {
    cell_group_cfg_bytes[i / 2] = std::stoul(cell_group_cfg.substr(i, 2), nullptr, 16);
  }
  asn1::cbit_ref                     bref(cell_group_cfg_bytes.data(), cell_group_cfg_bytes.size());
  asn1::rrc_nr_r17::cell_group_cfg_s cell_group;
  if (cell_group.unpack(bref) != asn1::SRSASN_SUCCESS) {
    asn1::log_error("Error unpacking cell group config");
    return false;
  }
  if (!update_phy_cfg_from_cell_cfg(phy_cfg, cell_group.sp_cell_cfg)) {
    asn1::log_error("Error updating phy cfg from cell group config");
    return false;
  }
  return true;
}

/* Load RRC setup cell configuration from file and apply to phy cfg */
bool configure_phy_cfg_from_rrc_setup(srsran::phy_cfg_nr_t& phy_cfg,
                                      std::string&          filename,
                                      uint32_t              nbits,
                                      srslog::basic_logger& logger)
{
  std::vector<uint8_t> subpdu_raw(nbits);
  if (!read_raw_config(filename, subpdu_raw.data(), nbits)) {
    printf("Failed to read RRC setup from %s\n", filename.c_str());
    return false;
  }
  asn1::rrc_nr_r17::dl_ccch_msg_s dl_ccch_msg;
  if (!parse_to_dl_ccch_msg(subpdu_raw.data(), nbits, dl_ccch_msg)) {
    printf("Failed to parse DL-CCCH message\n");
    return false;
  }
  if (dl_ccch_msg.msg.c1().type().value != asn1::rrc_nr_r17::dl_ccch_msg_type_c::c1_c_::types::rrc_setup) {
    printf("Expected RRC setup message\n");
    return false;
  }

  asn1::rrc_nr_r17::cell_group_cfg_s cell_group;
  if (!extract_cell_group_cfg(dl_ccch_msg, cell_group)) {
    printf("Failed to extract cell group config\n");
    return false;
  }

  if (cell_group.sp_cell_cfg_present) {
    if (!update_phy_cfg_from_cell_cfg(phy_cfg, cell_group.sp_cell_cfg)) {
      printf("Failed to update phy cfg from cell cfg\n");
      return false;
    }
  }
  if (cell_group.phys_cell_group_cfg_present) {
    switch (cell_group.phys_cell_group_cfg.pdsch_harq_ack_codebook) {
      case asn1::rrc_nr_r17::phys_cell_group_cfg_s::pdsch_harq_ack_codebook_opts::dyn:
        phy_cfg.harq_ack.harq_ack_codebook = srsran_pdsch_harq_ack_codebook_dynamic;
        break;
      case asn1::rrc_nr_r17::phys_cell_group_cfg_s::pdsch_harq_ack_codebook_opts::semi_static:
        phy_cfg.harq_ack.harq_ack_codebook = srsran_pdsch_harq_ack_codebook_semi_static;
        break;
      case asn1::rrc_nr_r17::phys_cell_group_cfg_s::pdsch_harq_ack_codebook_opts::nulltype:
        phy_cfg.harq_ack.harq_ack_codebook = srsran_pdsch_harq_ack_codebook_none;
        break;
      default:
        asn1::log_warning("Invalid option for pdsch_harq_ack_codebook %s",
                          cell_group.phys_cell_group_cfg.pdsch_harq_ack_codebook.to_string());
        return false;
    }
  }
  return true;
}

static void sliv_to_s_and_l(uint32_t N, uint32_t v, uint32_t* S, uint32_t* L)
{
  uint32_t low  = v % N;
  uint32_t high = v / N;
  if (high + 1 + low <= N) {
    *S = low;
    *L = high + 1;
  } else {
    *S = N - 1 - low;
    *L = N - high + 1;
  }
}

/* Apply MIB configuration to phy cfg */
bool update_phy_cfg_from_mib(srsran::phy_cfg_nr_t& phy_cfg, srsran_mib_nr_t& mib, uint32_t ncellid)
{
  phy_cfg.pdsch.typeA_pos = mib.dmrs_typeA_pos;
  phy_cfg.pdsch.scs_cfg   = mib.scs_common;
  phy_cfg.pusch.scs_cfg   = mib.scs_common;
  phy_cfg.carrier.pci     = ncellid;

  /* Get pointA and SSB absolute frequencies */
  double pointA_abs_freq_Hz = phy_cfg.carrier.dl_center_frequency_hz -
                              phy_cfg.carrier.nof_prb * SRSRAN_NRE * SRSRAN_SUBC_SPACING_NR(phy_cfg.carrier.scs) / 2;
  double ssb_abs_freq_Hz    = phy_cfg.carrier.ssb_center_freq_hz;
  /* Calculate integer SSB to pointA frequency offset in Hz */
  uint32_t ssb_pointA_freq_offset_Hz =
      (ssb_abs_freq_Hz > pointA_abs_freq_Hz) ? (uint32_t)(ssb_abs_freq_Hz - pointA_abs_freq_Hz) : 0;
  /* Create coreset0 */
  if (srsran_coreset_zero(phy_cfg.carrier.pci,
                          ssb_pointA_freq_offset_Hz,
                          phy_cfg.ssb.scs,
                          phy_cfg.carrier.scs,
                          mib.coreset0_idx,
                          &phy_cfg.pdcch.coreset[0])) {
    return false;
  }
  phy_cfg.pdcch.coreset_present[0] = true;

  /* Create SearchSpace0 */
  srsran::make_phy_search_space0_cfg(&phy_cfg.pdcch.search_space[0]);
  phy_cfg.pdcch.search_space_present[0] = true;
  return true;
}

/* Apply SIB1 configuration to phy cfg */
void update_phy_cfg_from_sib1(srsran::phy_cfg_nr_t& phy_cfg, asn1::rrc_nr_r17::sib1_s& sib1)
{
  if (!sib1.serving_cell_cfg_common_present) {
    asn1::log_error("Serving cell config common not present in SIB1, cannot update phy cfg");
    return;
  }
  auto& serv_cell_cfg = sib1.serving_cell_cfg_common;
  /* Apply PDSCH Config Common */
  if (serv_cell_cfg.dl_cfg_common.init_dl_bwp.pdsch_cfg_common_present) {
    if (!fill_pdsch_cfg_common(serv_cell_cfg.dl_cfg_common.init_dl_bwp.pdsch_cfg_common.setup(), &phy_cfg.pdsch)) {
      asn1::log_warning("Failed to apply PDSCH Config Common from SIB1");
    }
  }

  /* Apply PUSCH Config Common */
  if (serv_cell_cfg.ul_cfg_common_present) {
    if (serv_cell_cfg.ul_cfg_common.init_ul_bwp.pusch_cfg_common_present) {
      if (!fill_pusch_cfg_common(serv_cell_cfg.ul_cfg_common.init_ul_bwp.pusch_cfg_common.setup(), &phy_cfg.pusch)) {
        asn1::log_warning("Failed to apply PUSCH Config Common from SIB1");
      }
    }
  }

  /* Apply Carrier Config */
  fill_phy_carrier_cfg(serv_cell_cfg, &phy_cfg.carrier);

  /* Apply PUCCH Config Common */
  fill_phy_pucch_cfg_common(serv_cell_cfg.ul_cfg_common.init_ul_bwp.pucch_cfg_common.setup(), &phy_cfg.pucch.common);

  /* Apply RACH Config Common */
  if (!make_phy_rach_cfg(serv_cell_cfg.ul_cfg_common.init_ul_bwp.rach_cfg_common.setup(),
                         serv_cell_cfg.tdd_ul_dl_cfg_common_present ? SRSRAN_DUPLEX_MODE_TDD : SRSRAN_DUPLEX_MODE_FDD,
                         &phy_cfg.prach)) {
    asn1::log_warning("Failed to apply RACH Config Common from SIB1");
  }
  if (phy_cfg.prach.enable_msg3_transform_precoder) {
    phy_cfg.pusch.enable_transform_precoder = true;
  }

  /* Apply PDCCH Config Common */
  if (!fill_phy_pdcch_cfg_common(serv_cell_cfg.dl_cfg_common.init_dl_bwp.pdcch_cfg_common.setup(), &phy_cfg.pdcch)) {
    asn1::log_warning("Failed to apply PDCCH Config Common from SIB1");
  }

  /* Apply SSB Config */
  fill_phy_ssb_cfg(sib1.serving_cell_cfg_common, &phy_cfg.ssb);

  /* Apply n-TimingAdvanceOffset */
  if (serv_cell_cfg.n_timing_advance_offset_present) {
    switch (serv_cell_cfg.n_timing_advance_offset.value) {
      case asn1::rrc_nr_r17::serving_cell_cfg_common_sib_s::n_timing_advance_offset_opts::n0:
        phy_cfg.t_offset = 0;
        break;
      case asn1::rrc_nr_r17::serving_cell_cfg_common_sib_s::n_timing_advance_offset_opts::n25600:
        phy_cfg.t_offset = 25600;
        break;
      case asn1::rrc_nr_r17::serving_cell_cfg_common_sib_s::n_timing_advance_offset_opts::n39936:
        phy_cfg.t_offset = 39936;
        break;
      default:
        phy_cfg.t_offset = 25600;
        break;
    }
  } else {
    phy_cfg.t_offset = 25600;
  }

  if (serv_cell_cfg.tdd_ul_dl_cfg_common_present) {
    make_phy_tdd_cfg(serv_cell_cfg.tdd_ul_dl_cfg_common, &phy_cfg.duplex);
  }
}

/* Apply pdcch_cfg in BWP-DownlinkDedicated */
static bool apply_dedicated_pdcch_cfg(srsran::phy_cfg_nr_t&                phy_cfg,
                                      const asn1::rrc_nr_r17::pdcch_cfg_s& dedicated_pdcch_cfg)
{
  if (dedicated_pdcch_cfg.coreset_to_add_mod_list.size() > 0) {
    for (uint32_t i = 0; i < dedicated_pdcch_cfg.coreset_to_add_mod_list.size(); i++) {
      srsran_coreset_t coreset;
      if (make_phy_coreset_cfg(dedicated_pdcch_cfg.coreset_to_add_mod_list[i], &coreset) == true) {
        if (dedicated_pdcch_cfg.coreset_to_add_mod_list[i].tci_present_in_dci_present) {
          phy_cfg.pdsch_tci = true;
        }
        phy_cfg.pdcch.coreset[coreset.id]         = coreset;
        phy_cfg.pdcch.coreset_present[coreset.id] = true;
      } else {
        asn1::log_warning("Warning while building coreset structure");
        return false;
      }
    }
  } else {
    asn1::log_warning("Option coreset_to_add_mod_list not present");
  }
  if (dedicated_pdcch_cfg.search_spaces_to_add_mod_list.size() > 0) {
    for (uint32_t i = 0; i < dedicated_pdcch_cfg.search_spaces_to_add_mod_list.size(); i++) {
      srsran_search_space_t search_space;
      if (make_phy_search_space_cfg(dedicated_pdcch_cfg.search_spaces_to_add_mod_list[i], &search_space) == true) {
        phy_cfg.pdcch.dedicated_search_space[search_space.id]         = search_space;
        phy_cfg.pdcch.dedicated_search_space_present[search_space.id] = true;
      } else {
        asn1::log_warning("Warning while building search_space structure id=%d", i);
        return false;
      }
    }
  } else {
    asn1::log_warning("Option search_spaces_to_add_mod_list not present");
    return false;
  }
  return true;
}

static bool apply_dedicated_pdsch_cfg(srsran::phy_cfg_nr_t&                           phy_cfg,
                                      const asn1::rrc_nr_r17::pdsch_cfg_s&            dedicated_pdsch_cfg,
                                      std::map<uint32_t, srsran_csi_rs_zp_resource_t> csi_rs_zp_res)
{
  // Set the modulation scheme table
  if (dedicated_pdsch_cfg.mcs_table_present) {
    switch (dedicated_pdsch_cfg.mcs_table) {
      case asn1::rrc_nr_r17::pdsch_cfg_s::mcs_table_opts::qam256:
        phy_cfg.pdsch.mcs_table = srsran_mcs_table_256qam;
        break;
      case asn1::rrc_nr_r17::pdsch_cfg_s::mcs_table_opts::qam64_low_se:
        phy_cfg.pdsch.mcs_table = srsran_mcs_table_qam64LowSE;
        break;
      case asn1::rrc_nr_r17::pdsch_cfg_s::mcs_table_opts::nulltype:
        asn1::log_warning("Warning while selecting pdsch mcs_table");
        return false;
    }
  } else {
    phy_cfg.pdsch.mcs_table = srsran_mcs_table_64qam;
  }

  // Set the resource allocation type
  srsran_resource_alloc_t resource_alloc;
  if (make_phy_pdsch_alloc_type(dedicated_pdsch_cfg, &resource_alloc) == true) {
    phy_cfg.pdsch.alloc = resource_alloc;
  }

  // Set the DMRS Type A mapping
  if (dedicated_pdsch_cfg.dmrs_dl_for_pdsch_map_type_a_present) {
    const auto& dmrs_dl_cfg = dedicated_pdsch_cfg.dmrs_dl_for_pdsch_map_type_a.setup();
    // TS 38.214 5.1.6.2 - DM - RS reception procedure.
    if (dmrs_dl_cfg.dmrs_add_position_present) {
      make_phy_dmrs_dl_additional_pos(dmrs_dl_cfg, &phy_cfg.pdsch.dmrs_typeA.additional_pos);
      phy_cfg.pdsch.dmrs_typeA.present = true;
    }

    if (dmrs_dl_cfg.dmrs_type_present) {
      phy_cfg.pdsch.dmrs_type = srsran_dmrs_sch_type_2;
    }

    if (dmrs_dl_cfg.scrambling_id0_present) {
      phy_cfg.pdsch.dmrs_typeA.scrambling_id0         = dmrs_dl_cfg.scrambling_id0;
      phy_cfg.pdsch.dmrs_typeA.scrambling_id0_present = true;
    }
    if (dmrs_dl_cfg.scrambling_id1_present) {
      phy_cfg.pdsch.dmrs_typeA.scrambling_id1         = dmrs_dl_cfg.scrambling_id1;
      phy_cfg.pdsch.dmrs_typeA.scrambling_id1_present = true;
    }
  }
  // Set the DMRS Type B mapping
  else if (dedicated_pdsch_cfg.dmrs_dl_for_pdsch_map_type_b_present) {
    const auto& dmrs_dl_cfg = dedicated_pdsch_cfg.dmrs_dl_for_pdsch_map_type_b.setup();
    if (dmrs_dl_cfg.dmrs_add_position_present) {
      make_phy_dmrs_dl_additional_pos(dmrs_dl_cfg, &phy_cfg.pdsch.dmrs_typeB.additional_pos);
      phy_cfg.pdsch.dmrs_typeB.present = true;
    }

    if (dmrs_dl_cfg.dmrs_type_present) {
      phy_cfg.pdsch.dmrs_type = srsran_dmrs_sch_type_2;
    }

    if (dmrs_dl_cfg.scrambling_id0_present) {
      phy_cfg.pdsch.dmrs_typeB.scrambling_id0         = dmrs_dl_cfg.scrambling_id0;
      phy_cfg.pdsch.dmrs_typeB.scrambling_id0_present = true;
    }
    if (dmrs_dl_cfg.scrambling_id1_present) {
      phy_cfg.pdsch.dmrs_typeB.scrambling_id1         = dmrs_dl_cfg.scrambling_id1;
      phy_cfg.pdsch.dmrs_typeB.scrambling_id1_present = true;
    }
  } else {
    asn1::log_warning("Neither dmrs_dl_for_pdsch_map_type A nor type B present");
  }

  phy_cfg.pdsch.rbg_size_cfg_1 = (dedicated_pdsch_cfg.rbg_size == asn1::rrc_nr_r17::pdsch_cfg_s::rbg_size_opts::cfg1);

  if (dedicated_pdsch_cfg.prb_bundling_type.type() ==
      asn1::rrc_nr_r17::pdsch_cfg_s::prb_bundling_type_c_::types_opts::static_bundling) {
    phy_cfg.prb_dynamic_bundling = false;
    phy_cfg.prb_bundle_size      = dedicated_pdsch_cfg.prb_bundling_type.static_bundling().bundle_size;
  } else if (dedicated_pdsch_cfg.prb_bundling_type.type() ==
             asn1::rrc_nr_r17::pdsch_cfg_s::prb_bundling_type_c_::types_opts::dyn_bundling) {
    phy_cfg.prb_dynamic_bundling = true;
    phy_cfg.prb_bundle_size      = 0; // Not used in dynamic bundling
  }

  if (dedicated_pdsch_cfg.aperiodic_zp_csi_rs_res_sets_to_add_mod_list.size() > 0) {
    phy_cfg.nof_aperiodic_zp = dedicated_pdsch_cfg.aperiodic_zp_csi_rs_res_sets_to_add_mod_list.size();
  }
  if (dedicated_pdsch_cfg.vrb_to_prb_interleaver_present) {
    phy_cfg.pdsch_inter_prb_to_prb = dedicated_pdsch_cfg.vrb_to_prb_interleaver_present;
  }
  if (dedicated_pdsch_cfg.rate_match_pattern_group1.size() > 0) {
    phy_cfg.pdsch_rm_pattern1 = true;
  }
  if (dedicated_pdsch_cfg.rate_match_pattern_group2.size() > 0) {
    phy_cfg.pdsch_rm_pattern2 = true;
  }
  if (dedicated_pdsch_cfg.max_nrof_code_words_sched_by_dci_present) {
    phy_cfg.pdsch_2cw = dedicated_pdsch_cfg.max_nrof_code_words_sched_by_dci ==
                        asn1::rrc_nr_r17::pdsch_cfg_s::max_nrof_code_words_sched_by_dci_opts::n2;
  }

  // Set the time domain allocation list
  if (dedicated_pdsch_cfg.pdsch_time_domain_alloc_list_present) {
    auto& time_domain_alloc_list        = dedicated_pdsch_cfg.pdsch_time_domain_alloc_list.setup();
    phy_cfg.pdsch.nof_dedicated_time_ra = 0;
    for (uint32_t i = 0; i < time_domain_alloc_list.size(); i++) {
      srsran_sch_time_ra_t time_domain_alloc;
      if (make_phy_common_time_ra(time_domain_alloc_list[i], &time_domain_alloc) == true) {
        phy_cfg.pdsch.dedicated_time_ra[i]  = time_domain_alloc;
        phy_cfg.pdsch.nof_dedicated_time_ra = i + 1;
      } else {
        asn1::log_warning("Warning while building pdsch_time_domain_alloc_list");
        return false;
      }
    }
  }

  if (dedicated_pdsch_cfg.zp_csi_rs_res_to_add_mod_list.size() > 0) {
    for (uint32_t i = 0; i < dedicated_pdsch_cfg.zp_csi_rs_res_to_add_mod_list.size(); i++) {
      const auto& zp_res_cfg = dedicated_pdsch_cfg.zp_csi_rs_res_to_add_mod_list[i];

      srsran_csi_rs_zp_resource_t zp_csi_rs_resource;
      if (make_phy_zp_csi_rs_resource(zp_res_cfg, &zp_csi_rs_resource) == true) {
        csi_rs_zp_res[zp_csi_rs_resource.id] = zp_csi_rs_resource;
      } else {
        asn1::log_warning("Skipping unsupported or invalid ZP-CSI-RS resource id=%u", zp_res_cfg.zp_csi_rs_res_id);
      }
    }
  }

  if (dedicated_pdsch_cfg.p_zp_csi_rs_res_set_present) {
    if (csi_rs_zp_res.empty()) {
      asn1::log_warning("No supported ZP-CSI-RS resources available, skipping p_zp_csi_rs_res_set");
      return true;
    }

    if (dedicated_pdsch_cfg.p_zp_csi_rs_res_set.type() ==
        asn1::setup_release_c<asn1::rrc_nr_r17::zp_csi_rs_res_set_s>::types_opts::setup) {
      memset(&phy_cfg.pdsch.p_zp_csi_rs_set, 0, sizeof(phy_cfg.pdsch.p_zp_csi_rs_set));
      phy_cfg.pdsch.p_zp_csi_rs_set.count = 0;
      for (uint32_t i = 0; i < dedicated_pdsch_cfg.p_zp_csi_rs_res_set.setup().zp_csi_rs_res_id_list.size(); i++) {
        uint8_t res = dedicated_pdsch_cfg.p_zp_csi_rs_res_set.setup().zp_csi_rs_res_id_list[i];
        if (csi_rs_zp_res.find(res) == csi_rs_zp_res.end()) {
          asn1::log_warning("Skipping missing/unsupported p_zp_csi_rs_res id=%u", res);
          continue;
        }
        phy_cfg.pdsch.p_zp_csi_rs_set.data[i] = csi_rs_zp_res[res];
        phy_cfg.pdsch.p_zp_csi_rs_set.count += 1;
      }
    } else {
      asn1::log_warning("Option p_zp_csi_rs_res_set not of type setup");
      return false;
    }
  }
  return true;
}

/* Apply pusch_cfg in BWP-UplinkDedicated */
static bool apply_dedicated_pusch_cfg(srsran::phy_cfg_nr_t&                phy_cfg,
                                      const asn1::rrc_nr_r17::pusch_cfg_s& dedicated_pusch_cfg)
{
  // Set the modulation scheme table
  if (dedicated_pusch_cfg.mcs_table_present) {
    switch (dedicated_pusch_cfg.mcs_table) {
      case asn1::rrc_nr_r17::pusch_cfg_s::mcs_table_opts::qam256:
        phy_cfg.pusch.mcs_table = srsran_mcs_table_256qam;
        break;
      case asn1::rrc_nr_r17::pusch_cfg_s::mcs_table_opts::qam64_low_se:
        phy_cfg.pusch.mcs_table = srsran_mcs_table_qam64LowSE;
        break;
      case asn1::rrc_nr_r17::pusch_cfg_s::mcs_table_opts::nulltype:
        asn1::log_warning("Warning while selecting pusch mcs_table");
        return false;
    }
  } else {
    // If the field is absent the UE applies the value 64QAM.
    phy_cfg.pusch.mcs_table = srsran_mcs_table_64qam;
  }

  if (dedicated_pusch_cfg.max_rank_present) {
    phy_cfg.nof_ul_layers = dedicated_pusch_cfg.max_rank;
  } else {
    phy_cfg.nof_ul_layers = 1;
  }

  if (dedicated_pusch_cfg.freq_hop_present) {
    phy_cfg.enable_hopping = dedicated_pusch_cfg.freq_hop;
  }

  // Set the resource allocation type
  srsran_resource_alloc_t resource_alloc;
  if (make_phy_pusch_alloc_type(dedicated_pusch_cfg, &resource_alloc) == true) {
    phy_cfg.pusch.alloc = resource_alloc;
  }

  // Set the DMRS Type A additional position
  if (dedicated_pusch_cfg.dmrs_ul_for_pusch_map_type_a_present) {
    const auto& dmrs_ul_cfg = dedicated_pusch_cfg.dmrs_ul_for_pusch_map_type_a.setup();
    // See TS 38.331, DMRS-UplinkConfig. Also, see TS 38.214, 6.2.2 - UE DM-RS transmission procedure.
    if (dmrs_ul_cfg.dmrs_add_position_present) {
      make_phy_dmrs_ul_additional_pos(dmrs_ul_cfg, &phy_cfg.pusch.dmrs_typeA.additional_pos);
      phy_cfg.pusch.dmrs_typeA.present = true;
    }

    if (dmrs_ul_cfg.dmrs_type_present) {
      phy_cfg.pusch.dmrs_type = srsran_dmrs_sch_type_2;
    }

    if (dmrs_ul_cfg.max_len_present) {
      phy_cfg.pusch.dmrs_max_length = srsran_dmrs_sch_len_2;
    }

    if (dmrs_ul_cfg.phase_tracking_rs_present) {
      phy_cfg.pusch_ptrs = true;
    }

    if (dmrs_ul_cfg.transform_precoding_disabled_present) {
      if (dmrs_ul_cfg.transform_precoding_disabled.scrambling_id0_present) {
        phy_cfg.pusch.dmrs_typeA.scrambling_id0         = dmrs_ul_cfg.transform_precoding_disabled.scrambling_id0;
        phy_cfg.pusch.dmrs_typeA.scrambling_id0_present = true;
      }
      if (dmrs_ul_cfg.transform_precoding_disabled.scrambling_id1_present) {
        phy_cfg.pusch.dmrs_typeA.scrambling_id1         = dmrs_ul_cfg.transform_precoding_disabled.scrambling_id1;
        phy_cfg.pusch.dmrs_typeA.scrambling_id1_present = true;
      }
    }
  } else if (dedicated_pusch_cfg.dmrs_ul_for_pusch_map_type_b_present) {
    const auto& dmrs_ul_cfg = dedicated_pusch_cfg.dmrs_ul_for_pusch_map_type_b.setup();
    // See TS 38.331, DMRS-UplinkConfig. Also, see TS 38.214, 6.2.2 - UE DM-RS transmission procedure.
    if (dmrs_ul_cfg.dmrs_add_position_present) {
      make_phy_dmrs_ul_additional_pos(dmrs_ul_cfg, &phy_cfg.pusch.dmrs_typeB.additional_pos);
      phy_cfg.pusch.dmrs_typeB.present = true;
    }

    if (dmrs_ul_cfg.dmrs_type_present) {
      phy_cfg.pusch.dmrs_type = srsran_dmrs_sch_type_2;
    }

    if (dmrs_ul_cfg.max_len_present) {
      phy_cfg.pusch.dmrs_max_length = srsran_dmrs_sch_len_2;
    }

    if (dmrs_ul_cfg.phase_tracking_rs_present) {
      phy_cfg.pusch_ptrs = true;
    }

    if (dmrs_ul_cfg.transform_precoding_disabled_present) {
      if (dmrs_ul_cfg.transform_precoding_disabled.scrambling_id0_present) {
        phy_cfg.pusch.dmrs_typeB.scrambling_id0         = dmrs_ul_cfg.transform_precoding_disabled.scrambling_id0;
        phy_cfg.pusch.dmrs_typeB.scrambling_id0_present = true;
      }
      if (dmrs_ul_cfg.transform_precoding_disabled.scrambling_id1_present) {
        phy_cfg.pusch.dmrs_typeB.scrambling_id1         = dmrs_ul_cfg.transform_precoding_disabled.scrambling_id1;
        phy_cfg.pusch.dmrs_typeB.scrambling_id1_present = true;
      }
    }
  } else {
    asn1::log_warning("Neither dmrs_ul_for_pusch_map_type A nor type B present");
  }

  // Set the time domain resource allocation list
  if (dedicated_pusch_cfg.pusch_time_domain_alloc_list_present) {
    auto& time_domain_alloc_list        = dedicated_pusch_cfg.pusch_time_domain_alloc_list.setup();
    phy_cfg.pusch.nof_dedicated_time_ra = 0;
    for (uint32_t i = 0; i < time_domain_alloc_list.size(); i++) {
      srsran_sch_time_ra_t time_dom_alloc;
      if (make_phy_common_time_ra(time_domain_alloc_list[i], &time_dom_alloc) == true) {
        phy_cfg.pusch.dedicated_time_ra[i]  = time_dom_alloc;
        phy_cfg.pusch.nof_dedicated_time_ra = i + 1;
      } else {
        asn1::log_warning("Warning while building time domain allocation structure id=%d", i);
        return false;
      }
    }
  }

  // Set the transform precoder flag
  if (dedicated_pusch_cfg.transform_precoder_present) {
    switch (dedicated_pusch_cfg.transform_precoder) {
      case asn1::rrc_nr_r17::pusch_cfg_s::transform_precoder_opts::enabled:
        phy_cfg.pusch.enable_transform_precoder = true;
        break;
      case asn1::rrc_nr_r17::pusch_cfg_s::transform_precoder_opts::disabled:
        phy_cfg.pusch.enable_transform_precoder = false;
        break;
      default:
        phy_cfg.pusch.enable_transform_precoder = false;
        break;
    }
  }

  if (phy_cfg.pusch.enable_transform_precoder && dedicated_pusch_cfg.mcs_table_transform_precoder_present) {
    switch (dedicated_pusch_cfg.mcs_table_transform_precoder) {
      case asn1::rrc_nr_r17::pusch_cfg_s::mcs_table_transform_precoder_opts::qam64_low_se:
        phy_cfg.pusch.mcs_table_transform_precoder = srsran_mcs_table_qam64LowSE;
        break;
      case asn1::rrc_nr_r17::pusch_cfg_s::mcs_table_transform_precoder_opts::qam256:
        phy_cfg.pusch.mcs_table_transform_precoder = srsran_mcs_table_256qam;
        break;
      default:
        phy_cfg.pusch.mcs_table_transform_precoder = srsran_mcs_table_N;
        asn1::log_warning("Warning while selecting mcs_table_transform_precoder");
        break;
    }
  }

  if (dedicated_pusch_cfg.tx_cfg_present) {
    if (dedicated_pusch_cfg.tx_cfg == asn1::rrc_nr_r17::pusch_cfg_s::tx_cfg_opts::codebook) {
      phy_cfg.pusch_tx_cfg_non_codebook = false;
    } else if (dedicated_pusch_cfg.tx_cfg == asn1::rrc_nr_r17::pusch_cfg_s::tx_cfg_opts::non_codebook) {
      phy_cfg.pusch_tx_cfg_non_codebook = true;
    }
  }

  // Set the UCI on PUSCH configuration
  if (dedicated_pusch_cfg.uci_on_pusch_present) {
    if (dedicated_pusch_cfg.uci_on_pusch.setup().beta_offsets_present) {
      if (dedicated_pusch_cfg.uci_on_pusch.setup().beta_offsets.type() ==
          asn1::rrc_nr_r17::uci_on_pusch_s::beta_offsets_c_::types_opts::semi_static) {
        srsran_beta_offsets_t beta_offsets;
        if (make_phy_beta_offsets(dedicated_pusch_cfg.uci_on_pusch.setup().beta_offsets.semi_static(), &beta_offsets) ==
            true) {
          phy_cfg.pusch.beta_offsets  = beta_offsets;
          phy_cfg.pusch_dynamic_betas = false;
        } else {
          asn1::log_warning("Warning while building beta_offsets structure");
          return false;
        }
      } else if (dedicated_pusch_cfg.uci_on_pusch.setup().beta_offsets.type() ==
                 asn1::rrc_nr_r17::uci_on_pusch_s::beta_offsets_c_::types_opts::dyn) {
        phy_cfg.pusch_dynamic_betas = true;
      }
      if (dedicated_pusch_cfg.uci_on_pusch_present) {
        if (make_phy_pusch_scaling(dedicated_pusch_cfg.uci_on_pusch.setup(), &phy_cfg.pusch.scaling) == false) {
          asn1::log_warning("Warning while building scaling structure");
          return false;
        }
      }
    } else {
      asn1::log_warning("Option beta_offsets not present");
    }
  } else {
    asn1::log_warning("Option uci_on_pusch not present");
  }
  return true;
};

static bool apply_dedicated_srs_cfg(srsran::phy_cfg_nr_t& phy_cfg, asn1::rrc_nr_r17::srs_cfg_s& dedicated_srs_cfg)
{
  uint32_t max_srs_in_set = 0;
  for (uint32_t i = 0; i < dedicated_srs_cfg.srs_res_set_to_add_mod_list.size(); i++) {
    auto& rs = dedicated_srs_cfg.srs_res_set_to_add_mod_list[i];
    if (rs.srs_res_id_list.size() > max_srs_in_set) {
      max_srs_in_set = rs.srs_res_id_list.size();
    }
  }
  if (max_srs_in_set > 0) {
    phy_cfg.nof_srs = max_srs_in_set;
  }

  uint32_t max_srs_port = 1;
  for (uint32_t i = 0; i < dedicated_srs_cfg.srs_res_to_add_mod_list.size(); i++) {
    auto&   res   = dedicated_srs_cfg.srs_res_to_add_mod_list[i];
    uint8_t ports = res.nrof_srs_ports.to_number();
    if (ports > max_srs_port) {
      max_srs_port = ports;
    }
  }
  phy_cfg.nof_srs_ports = max_srs_port;
  return true;
}

static bool apply_ul_bwp_Dedicated(asn1::rrc_nr_r17::bwp_ul_ded_s& bwp_cfg, srsran::phy_cfg_nr_t& phy_cfg)
{
  bool success = true;

  if (bwp_cfg.pucch_cfg_present) {
    if (bwp_cfg.pucch_cfg.type() == asn1::setup_release_c<asn1::rrc_nr_r17::pucch_cfg_s>::types_opts::setup) {
      srsran::static_circular_map<uint32_t, srsran_pucch_nr_resource_t, 128UL> pucch_res_list;
      if (!apply_dedicated_pucch_cfg(phy_cfg, bwp_cfg.pucch_cfg.setup(), pucch_res_list)) {
        success = false;
      }
    } else {
      asn1::log_warning("Option pucch_cfg not of type setup");
      return false;
    }
  }

  if (bwp_cfg.pusch_cfg_present) {
    if (bwp_cfg.pusch_cfg.type() == asn1::setup_release_c<asn1::rrc_nr_r17::pusch_cfg_s>::types_opts::setup) {
      if (!apply_dedicated_pusch_cfg(phy_cfg, bwp_cfg.pusch_cfg.setup())) {
        success = false;
      }
    } else {
      asn1::log_warning("Option pusch_cfg not of type setup");
      return false;
    }
  }

  if (bwp_cfg.srs_cfg_present) {
    if (bwp_cfg.srs_cfg.type() == asn1::setup_release_c<asn1::rrc_nr_r17::srs_cfg_s>::types_opts::setup) {
      if (!apply_dedicated_srs_cfg(phy_cfg, bwp_cfg.srs_cfg.setup())) {
        success = false;
      }
    } else {
      asn1::log_warning("Option srs_cfg not of type setup");
      return false;
    }
  }
  return success;
}

static bool apply_dl_bwp_Dedicated(asn1::rrc_nr_r17::bwp_dl_ded_s& bwp_cfg, srsran::phy_cfg_nr_t& phy_cfg)
{
  bool success = true;
  if (bwp_cfg.pdcch_cfg_present) {
    if (bwp_cfg.pdcch_cfg.type() == asn1::setup_release_c<asn1::rrc_nr_r17::pdcch_cfg_s>::types_opts::setup) {
      if (!apply_dedicated_pdcch_cfg(phy_cfg, bwp_cfg.pdcch_cfg.setup())) {
        asn1::log_warning("Couldn't apply pdcch_cfg in BWP-DownlinkDedicated");
        success = false;
      }
    } else {
      asn1::log_warning("Option pdcch_cfg not of type setup");
      success = false;
    }
  }

  if (bwp_cfg.pdsch_cfg_present) {
    if (bwp_cfg.pdsch_cfg.type() == asn1::setup_release_c<asn1::rrc_nr_r17::pdsch_cfg_s>::types_opts::setup) {
      if (!apply_dedicated_pdsch_cfg(phy_cfg, bwp_cfg.pdsch_cfg.setup(), phy_cfg.csi_rs_zp_res)) {
        asn1::log_warning("Couldn't apply pdsch_cfg in BWP-DownlinkDedicated");
        success = false;
      }
    } else {
      asn1::log_warning("Option pdsch_cfg not of type setup");
      success = false;
    }
  }
  return success;
}

static bool apply_dl_bwp_Common(asn1::rrc_nr_r17::bwp_dl_common_s& bwp_cfg, srsran::phy_cfg_nr_t& phy_cfg)
{
  bool success               = true;
  phy_cfg.dl_location_and_bw = bwp_cfg.generic_params.location_and_bw;
  sliv_to_s_and_l(275, phy_cfg.dl_location_and_bw, &phy_cfg.dl_bwp_start, &phy_cfg.dl_bwp_size);

  if (bwp_cfg.pdcch_cfg_common_present) {
    if (bwp_cfg.pdcch_cfg_common.type() ==
        asn1::setup_release_c<asn1::rrc_nr_r17::pdcch_cfg_common_s>::types_opts::setup) {
      if (!fill_phy_pdcch_cfg_common(bwp_cfg.pdcch_cfg_common.setup(), &phy_cfg.pdcch)) {
        asn1::log_warning("Couldn't apply pdcch_cfg_common in BWP-DownlinkCommon");
        success = false;
      }
    } else {
      asn1::log_warning("Option pdcch_cfg_common not of type setup");
      success = false;
    }
  }

  if (bwp_cfg.pdsch_cfg_common_present) {
    if (bwp_cfg.pdsch_cfg_common.type() ==
        asn1::setup_release_c<asn1::rrc_nr_r17::pdsch_cfg_common_s>::types_opts::setup) {
      if (!fill_phy_pdsch_cfg_common(bwp_cfg.pdsch_cfg_common.setup(), &phy_cfg.pdsch)) {
        asn1::log_warning("Couldn't apply pdsch_cfg_common in BWP-DownlinkCommon");
        success = false;
      }
    } else {
      asn1::log_warning("Option pdsch_cfg_common not of type setup");
      success = false;
    }
  }

  return success;
}

static bool apply_ul_bwp_Common(asn1::rrc_nr_r17::bwp_ul_common_s& bwp_cfg, srsran::phy_cfg_nr_t& phy_cfg)
{
  bool success               = true;
  phy_cfg.ul_location_and_bw = bwp_cfg.generic_params.location_and_bw;
  sliv_to_s_and_l(275, phy_cfg.ul_location_and_bw, &phy_cfg.ul_bwp_start, &phy_cfg.ul_bwp_size);
  if (bwp_cfg.rach_cfg_common_present) {
    if (bwp_cfg.rach_cfg_common.type() ==
        asn1::setup_release_c<asn1::rrc_nr_r17::rach_cfg_common_s>::types_opts::setup) {
      if (!make_phy_rach_cfg(bwp_cfg.rach_cfg_common.setup(), phy_cfg.duplex.mode, &phy_cfg.prach)) {
        asn1::log_warning("Couldn't apply rach_cfg_common in BWP-UplinkCommon");
        success = false;
      }
    } else {
      asn1::log_warning("Option rach_cfg_common not of type setup");
      success = false;
    }
  }

  if (bwp_cfg.pusch_cfg_common_present) {
    if (bwp_cfg.pusch_cfg_common.type() ==
        asn1::setup_release_c<asn1::rrc_nr_r17::pusch_cfg_common_s>::types_opts::setup) {
      if (!fill_phy_pusch_cfg_common(bwp_cfg.pusch_cfg_common.setup(), &phy_cfg.pusch)) {
        asn1::log_warning("Couldn't apply pusch_cfg_common in BWP-UplinkCommon");
        success = false;
      }
      if (phy_cfg.prach.enable_msg3_transform_precoder) {
        phy_cfg.pusch.enable_transform_precoder = true;
      }
    } else {
      asn1::log_warning("Option pusch_cfg_common not of type setup");
      success = false;
    }
  }

  if (bwp_cfg.pucch_cfg_common_present) {
    if (bwp_cfg.pucch_cfg_common.type() ==
        asn1::setup_release_c<asn1::rrc_nr_r17::pucch_cfg_common_s>::types_opts::setup) {
      fill_phy_pucch_cfg_common(bwp_cfg.pucch_cfg_common.setup(), &phy_cfg.pucch.common);
    } else {
      asn1::log_warning("Option pucch_cfg_common not of type setup");
      success = false;
    }
  }

  return success;
}

static bool apply_csi_meas_cfg(srsran::phy_cfg_nr_t&                             phy_cfg,
                               asn1::rrc_nr_r17::csi_meas_cfg_s&                 csi_meas_cfg,
                               std::map<uint32_t, srsran_csi_rs_nzp_resource_t>& csi_rs_nzp_res)
{
  bool success = true;
  for (uint32_t i = 0; i < csi_meas_cfg.nzp_csi_rs_res_to_add_mod_list.size(); i++) {
    srsran_csi_rs_nzp_resource_t csi_rs_nzp_resource;
    if (make_phy_nzp_csi_rs_resource(csi_meas_cfg.nzp_csi_rs_res_to_add_mod_list[i], &csi_rs_nzp_resource)) {
      csi_rs_nzp_res[csi_rs_nzp_resource.id] = csi_rs_nzp_resource;
    } else {
      asn1::log_warning("Warning while building nzp_csi_rs resource, skipping");
    }
  }

  for (uint32_t i = 0; i < csi_meas_cfg.nzp_csi_rs_res_set_to_add_mod_list.size(); i++) {
    uint32_t set_id = csi_meas_cfg.nzp_csi_rs_res_set_to_add_mod_list[i].nzp_csi_res_set_id;
    for (uint32_t j = 0; j < csi_meas_cfg.nzp_csi_rs_res_set_to_add_mod_list[i].nzp_csi_rs_res.size(); j++) {
      uint8_t res = csi_meas_cfg.nzp_csi_rs_res_set_to_add_mod_list[i].nzp_csi_rs_res[j];
      if (csi_rs_nzp_res.find(res) != csi_rs_nzp_res.end()) {
        phy_cfg.pdsch.nzp_csi_rs_sets[set_id].data[j] = csi_rs_nzp_res[res];
        phy_cfg.pdsch.nzp_csi_rs_sets[set_id].count += 1;
      } else {
        asn1::log_warning("Cannot find nzp_csi_rs_res %d in temporally stored csi_rs_nzp_res, skipping", res);
      }
    }
    if (csi_meas_cfg.nzp_csi_rs_res_set_to_add_mod_list[i].trs_info_present) {
      phy_cfg.pdsch.nzp_csi_rs_sets[set_id].trs_info = true;
    }
  }
  return success;
}

/* Apply cell configuration to phy cfg, Copy from file rrc_nr.cc */
bool update_phy_cfg_from_cell_cfg(srsran::phy_cfg_nr_t& phy_cfg, asn1::rrc_nr_r17::sp_cell_cfg_s& sp_cell_cfg)
{
  bool success = true;
  if (!sp_cell_cfg.sp_cell_cfg_ded_present) {
    asn1::log_warning("Option sp_cell_cfg_ded not present, skipping dedicated config application");
    return false;
  }

  if (sp_cell_cfg.sp_cell_cfg_ded.csi_meas_cfg_present &&
      sp_cell_cfg.sp_cell_cfg_ded.csi_meas_cfg.type() ==
          asn1::setup_release_c<asn1::rrc_nr_r17::csi_meas_cfg_s>::types_opts::setup) {
    if (sp_cell_cfg.sp_cell_cfg_ded.csi_meas_cfg.setup().report_trigger_size_present) {
      phy_cfg.report_trigger_size = sp_cell_cfg.sp_cell_cfg_ded.csi_meas_cfg.setup().report_trigger_size;
    }
  }

  if (sp_cell_cfg.sp_cell_cfg_ded.pdsch_serving_cell_cfg_present) {
    auto& pdsch_serving_cell_cfg = sp_cell_cfg.sp_cell_cfg_ded.pdsch_serving_cell_cfg.setup();
    if (pdsch_serving_cell_cfg.code_block_group_tx_present) {
      phy_cfg.pdsch_cbg_flush = true;
      switch (pdsch_serving_cell_cfg.code_block_group_tx.setup().max_code_block_groups_per_transport_block) {
        case asn1::rrc_nr_r17::pdsch_code_block_group_tx_s::max_code_block_groups_per_transport_block_opts::n2:
          phy_cfg.pusch_nof_cbg = 2;
          phy_cfg.pdsch_nof_cbg = 2;
          break;
        case asn1::rrc_nr_r17::pdsch_code_block_group_tx_s::max_code_block_groups_per_transport_block_opts::n4:
          phy_cfg.pusch_nof_cbg = 4;
          phy_cfg.pdsch_nof_cbg = 4;
          break;
        case asn1::rrc_nr_r17::pdsch_code_block_group_tx_s::max_code_block_groups_per_transport_block_opts::n6:
          phy_cfg.pusch_nof_cbg = 6;
          phy_cfg.pdsch_nof_cbg = 6;
          break;
        case asn1::rrc_nr_r17::pdsch_code_block_group_tx_s::max_code_block_groups_per_transport_block_opts::n8:
          phy_cfg.pusch_nof_cbg = 8;
          phy_cfg.pdsch_nof_cbg = 8;
          break;
        default:
          phy_cfg.pusch_nof_cbg = 0;
          phy_cfg.pdsch_nof_cbg = 0;
          break;
      }
    } else {
      phy_cfg.pusch_nof_cbg = 0;
      phy_cfg.pdsch_nof_cbg = 0;
    }
  }

  // Apply Downlink BWP config
  if (sp_cell_cfg.sp_cell_cfg_ded.first_active_dl_bwp_id_present) {
    uint8_t active_dl_bwp_id = sp_cell_cfg.sp_cell_cfg_ded.first_active_dl_bwp_id;
    phy_cfg.nof_dl_bwp       = sp_cell_cfg.sp_cell_cfg_ded.dl_bwp_to_add_mod_list.size();
    for (auto& bwp_cfg : sp_cell_cfg.sp_cell_cfg_ded.dl_bwp_to_add_mod_list) {
      if (bwp_cfg.bwp_id != active_dl_bwp_id) {
        asn1::log_debug("DL BWP id %u is not active, skipping", bwp_cfg.bwp_id);
        continue;
      }

      if (bwp_cfg.bwp_common_present) {
        if (!apply_dl_bwp_Common(bwp_cfg.bwp_common, phy_cfg)) {
          success = false;
        }
      }

      if (bwp_cfg.bwp_ded_present) {
        if (!apply_dl_bwp_Dedicated(bwp_cfg.bwp_ded, phy_cfg)) {
          success = false;
        }
      }
    }
  } else if (sp_cell_cfg.sp_cell_cfg_ded.init_dl_bwp_present) {
    asn1::log_warning("Option first_active_dl_bwp_id not present, applying init_dl_bwp config");
    if (!apply_dl_bwp_Dedicated(sp_cell_cfg.sp_cell_cfg_ded.init_dl_bwp, phy_cfg)) {
      success = false;
    }
  } else {
    asn1::log_warning("Option first_active_dl_bwp_id and init_dl_bwp not present, skipping DL BWP config application");
  }

  // Apply Uplink BWP config
  if (sp_cell_cfg.sp_cell_cfg_ded.ul_cfg_present) {
    if (sp_cell_cfg.sp_cell_cfg_ded.ul_cfg.first_active_ul_bwp_id_present) {
      uint8_t active_ul_bwp_id = sp_cell_cfg.sp_cell_cfg_ded.ul_cfg.first_active_ul_bwp_id;
      phy_cfg.nof_ul_bwp       = sp_cell_cfg.sp_cell_cfg_ded.ul_cfg.ul_bwp_to_add_mod_list.size();
      for (auto& bwp_cfg : sp_cell_cfg.sp_cell_cfg_ded.ul_cfg.ul_bwp_to_add_mod_list) {
        if (bwp_cfg.bwp_id != active_ul_bwp_id) {
          asn1::log_debug("UL BWP id %u is not active, skipping", bwp_cfg.bwp_id);
          continue;
        }
        if (bwp_cfg.bwp_common_present) {
          if (!apply_ul_bwp_Common(bwp_cfg.bwp_common, phy_cfg)) {
            success = false;
          }
        }

        if (bwp_cfg.bwp_ded_present) {
          if (!apply_ul_bwp_Dedicated(bwp_cfg.bwp_ded, phy_cfg)) {
            success = false;
          }
        }
      }
    } else if (sp_cell_cfg.sp_cell_cfg_ded.ul_cfg.init_ul_bwp_present) {
      asn1::log_warning("Option first_active_ul_bwp_id not present, applying init_ul_bwp config");
      if (!apply_ul_bwp_Dedicated(sp_cell_cfg.sp_cell_cfg_ded.ul_cfg.init_ul_bwp, phy_cfg)) {
        success = false;
      }
    } else {
      asn1::log_warning(
          "Option first_active_ul_bwp_id and init_ul_bwp not present, skipping UL BWP config application");
    }
  }

  if (sp_cell_cfg.sp_cell_cfg_ded.csi_meas_cfg_present) {
    if (!apply_csi_meas_cfg(phy_cfg, sp_cell_cfg.sp_cell_cfg_ded.csi_meas_cfg.setup(), phy_cfg.csi_rs_nzp_res)) {
      asn1::log_warning("Couldn't apply CSI measurement config in cell config");
      success = false;
    }
  }
  return success;
}

/* Decode dl_ccch_msg_s bytes to asn1 structure */
bool parse_to_dl_ccch_msg(uint8_t* data, uint32_t len, asn1::rrc_nr_r17::dl_ccch_msg_s& dl_ccch_msg)
{
  asn1::cbit_ref    bref(data, len);
  asn1::SRSASN_CODE err = dl_ccch_msg.unpack(bref);
  if (err != asn1::SRSASN_SUCCESS ||
      dl_ccch_msg.msg.type().value != asn1::rrc_nr_r17::dl_ccch_msg_type_c::types_opts::c1) {
    std::cerr << "Error unpacking DL-CCCH message\n";
    return false;
  }
  return true;
}

/* extract cell_group struct from rrc_setup */
bool extract_cell_group_cfg(asn1::rrc_nr_r17::dl_ccch_msg_s&    dl_ccch_msg,
                            asn1::rrc_nr_r17::cell_group_cfg_s& cell_group)
{
  asn1::rrc_nr_r17::rrc_setup_s& rrc_setup_msg = dl_ccch_msg.msg.c1().rrc_setup();
  asn1::cbit_ref                 bref_cg(rrc_setup_msg.crit_exts.rrc_setup().master_cell_group.data(),
                                         rrc_setup_msg.crit_exts.rrc_setup().master_cell_group.size());
  if (cell_group.unpack(bref_cg) != asn1::SRSASN_SUCCESS) {
    printf("Could not unpack master cell group config.\n");
    return false;
  }
  return true;
}

bool extract_cell_group_cfg(asn1::rrc_nr_r17::rrc_recfg_s& rrc_recfg, asn1::rrc_nr_r17::cell_group_cfg_s& cell_group)
{
  if (rrc_recfg.crit_exts.type().value != asn1::rrc_nr_r17::rrc_recfg_s::crit_exts_c_::types::rrc_recfg) {
    asn1::log_warning("RRCReconfiguration does not contain reconfigWithSync critical extension.\n");
    return false;
  }
  if (!rrc_recfg.crit_exts.rrc_recfg().non_crit_ext_present) {
    asn1::log_warning("RRCReconfiguration does not contain non-critical extensions.\n");
    return false;
  }
  size_t         cell_group_cfg_size = rrc_recfg.crit_exts.rrc_recfg().non_crit_ext.master_cell_group.size();
  uint8_t*       cell_group_cfg_data = rrc_recfg.crit_exts.rrc_recfg().non_crit_ext.master_cell_group.data();
  asn1::cbit_ref bref_cg(cell_group_cfg_data, cell_group_cfg_size);
  if (cell_group.unpack(bref_cg) != asn1::SRSASN_SUCCESS) {
    asn1::log_error("Could not unpack master cell group config.\n");
    return false;
  }
  return true;
}