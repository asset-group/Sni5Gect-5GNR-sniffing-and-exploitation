#include "shadower/utils/phy_cfg_utils.h"
#include "srsran/asn1/rrc_nr.h"
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
bool parse_to_sib1(uint8_t* data, uint32_t len, asn1::rrc_nr::sib1_s& sib1)
{
  asn1::rrc_nr::bcch_dl_sch_msg_s dlsch_msg;
  asn1::cbit_ref                  dlsch_bref(data, len);
  asn1::SRSASN_CODE               err = dlsch_msg.unpack(dlsch_bref);
  if (err != asn1::SRSASN_SUCCESS ||
      dlsch_msg.msg.type().value != asn1::rrc_nr::bcch_dl_sch_msg_type_c::types_opts::c1) {
    std::cerr << "Error unpacking BCCH-BCH message\n";
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
  if (phy_cfg.duplex.mode == SRSRAN_DUPLEX_MODE_TDD) {
    slot_cfg.idx = slot_idx + 1;
  } else {
    slot_cfg.idx = slot_idx;
  }
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

  asn1::rrc_nr::sib1_s sib1;
  if (!parse_to_sib1(sib1_raw.data(), nbits, sib1)) {
    printf("Failed to parse SIB1\n");
    return false;
  }
  update_phy_cfg_from_sib1(phy_cfg, sib1);
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
  asn1::rrc_nr::dl_ccch_msg_s dl_ccch_msg;
  if (!parse_to_dl_ccch_msg(subpdu_raw.data(), nbits, dl_ccch_msg)) {
    printf("Failed to parse DL-CCCH message\n");
    return false;
  }
  if (dl_ccch_msg.msg.c1().type().value != asn1::rrc_nr::dl_ccch_msg_type_c::c1_c_::types::rrc_setup) {
    printf("Expected RRC setup message\n");
    return false;
  }

  asn1::rrc_nr::cell_group_cfg_s cell_group;
  if (!extract_cell_group_cfg(dl_ccch_msg, cell_group)) {
    printf("Failed to extract cell group config\n");
    return false;
  }

  srsran::static_circular_map<uint32_t, srsran_pucch_nr_resource_t, 128> pucch_res_list;
  std::map<uint32_t, srsran_csi_rs_zp_resource_t>                        csi_rs_zp_res;
  std::map<uint32_t, srsran_csi_rs_nzp_resource_t>                       csi_rs_nzp_res;
  if (cell_group.sp_cell_cfg_present) {
    if (!update_phy_cfg_from_cell_cfg(
            phy_cfg, cell_group.sp_cell_cfg, pucch_res_list, csi_rs_zp_res, csi_rs_nzp_res, logger)) {
      printf("Failed to update phy cfg from cell cfg\n");
      return false;
    }
  }
  if (cell_group.phys_cell_group_cfg_present) {
    switch (cell_group.phys_cell_group_cfg.pdsch_harq_ack_codebook) {
      case asn1::rrc_nr::phys_cell_group_cfg_s::pdsch_harq_ack_codebook_opts::dynamic_value:
        phy_cfg.harq_ack.harq_ack_codebook = srsran_pdsch_harq_ack_codebook_dynamic;
        break;
      case asn1::rrc_nr::phys_cell_group_cfg_s::pdsch_harq_ack_codebook_opts::semi_static:
        phy_cfg.harq_ack.harq_ack_codebook = srsran_pdsch_harq_ack_codebook_semi_static;
        break;
      case asn1::rrc_nr::phys_cell_group_cfg_s::pdsch_harq_ack_codebook_opts::nulltype:
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

/* Apply MIB configuration to phy cfg */
bool update_phy_cfg_from_mib(srsran::phy_cfg_nr_t& phy_cfg, srsran_mib_nr_t& mib, uint32_t ncellid)
{
  phy_cfg.pdsch.typeA_pos = mib.dmrs_typeA_pos;
  phy_cfg.pdsch.scs_cfg   = mib.scs_common;
  phy_cfg.carrier.pci     = ncellid;

  /* Get pointA and SSB absolute frequencies */
  double pointA_abs_freq_Hz = phy_cfg.carrier.dl_center_frequency_hz -
                              phy_cfg.carrier.nof_prb * SRSRAN_NRE * SRSRAN_SUBC_SPACING_NR(phy_cfg.carrier.scs) / 2;
  double ssb_abs_freq_Hz = phy_cfg.carrier.ssb_center_freq_hz;
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
void update_phy_cfg_from_sib1(srsran::phy_cfg_nr_t& phy_cfg, asn1::rrc_nr::sib1_s& sib1)
{
  /* Apply PDSCH Config Common */
  if (sib1.serving_cell_cfg_common.dl_cfg_common.init_dl_bwp.pdsch_cfg_common.setup()
          .pdsch_time_domain_alloc_list.size() > 0) {
    if (!srsran::fill_phy_pdsch_cfg_common(
            sib1.serving_cell_cfg_common.dl_cfg_common.init_dl_bwp.pdsch_cfg_common.setup(), &phy_cfg.pdsch)) {
    }
  }

  /* Apply PUSCH Config Common */
  if (!srsran::fill_phy_pusch_cfg_common(
          sib1.serving_cell_cfg_common.ul_cfg_common.init_ul_bwp.pusch_cfg_common.setup(), &phy_cfg.pusch)) {
  }

  /* Apply PUCCH Config Common */
  srsran::fill_phy_pucch_cfg_common(sib1.serving_cell_cfg_common.ul_cfg_common.init_ul_bwp.pucch_cfg_common.setup(),
                                    &phy_cfg.pucch.common);

  /* Apply RACH Config Common */
  if (!srsran::make_phy_rach_cfg(sib1.serving_cell_cfg_common.ul_cfg_common.init_ul_bwp.rach_cfg_common.setup(),
                                 sib1.serving_cell_cfg_common.tdd_ul_dl_cfg_common_present ? SRSRAN_DUPLEX_MODE_TDD
                                                                                           : SRSRAN_DUPLEX_MODE_FDD,
                                 &phy_cfg.prach)) {
  }

  /* Apply PDCCH Config Common */
  srsran::fill_phy_pdcch_cfg_common(sib1.serving_cell_cfg_common.dl_cfg_common.init_dl_bwp.pdcch_cfg_common.setup(),
                                    &phy_cfg.pdcch);

  /* Apply Carrier Config */
  srsran::fill_phy_carrier_cfg(sib1.serving_cell_cfg_common, &phy_cfg.carrier);

  /* Apply SSB Config */
  srsran::fill_phy_ssb_cfg(sib1.serving_cell_cfg_common, &phy_cfg.ssb);
  /* Apply n-TimingAdvanceOffset */
  if (sib1.serving_cell_cfg_common.n_timing_advance_offset_present) {
    switch (sib1.serving_cell_cfg_common.n_timing_advance_offset.value) {
      case asn1::rrc_nr::serving_cell_cfg_common_sib_s::n_timing_advance_offset_opts::n0:
        phy_cfg.t_offset = 0;
        break;
      case asn1::rrc_nr::serving_cell_cfg_common_sib_s::n_timing_advance_offset_opts::n25600:
        phy_cfg.t_offset = 25600;
        break;
      case asn1::rrc_nr::serving_cell_cfg_common_sib_s::n_timing_advance_offset_opts::n39936:
        phy_cfg.t_offset = 39936;
        break;
      default:
        break;
    }
  } else {
    phy_cfg.t_offset = 25600;
  }
  if (sib1.serving_cell_cfg_common.tdd_ul_dl_cfg_common_present) {
    srsran::make_phy_tdd_cfg(sib1.serving_cell_cfg_common.tdd_ul_dl_cfg_common, &phy_cfg.duplex);
  }
}

/* pdcch configuration update from cell config */
static bool apply_sp_cell_init_dl_pdcch(srsran::phy_cfg_nr_t&            phy_cfg,
                                        const asn1::rrc_nr::pdcch_cfg_s& pdcch_cfg,
                                        srslog::basic_logger&            logger)
{
  if (pdcch_cfg.search_spaces_to_add_mod_list.size() > 0) {
    for (uint32_t i = 0; i < pdcch_cfg.search_spaces_to_add_mod_list.size(); i++) {
      srsran_search_space_t search_space;
      if (srsran::make_phy_search_space_cfg(pdcch_cfg.search_spaces_to_add_mod_list[i], &search_space) == true) {
        phy_cfg.pdcch.search_space[search_space.id]         = search_space;
        phy_cfg.pdcch.search_space_present[search_space.id] = true;
      } else {
        logger.warning("Warning while building search_space structure id=%d", i);
        return false;
      }
    }
  } else {
    logger.warning("Option search_spaces_to_add_mod_list not present");
    return false;
  }
  if (pdcch_cfg.ctrl_res_set_to_add_mod_list.size() > 0) {
    for (uint32_t i = 0; i < pdcch_cfg.ctrl_res_set_to_add_mod_list.size(); i++) {
      srsran_coreset_t coreset;
      if (srsran::make_phy_coreset_cfg(pdcch_cfg.ctrl_res_set_to_add_mod_list[i], &coreset) == true) {
        phy_cfg.pdcch.coreset[coreset.id]         = coreset;
        phy_cfg.pdcch.coreset_present[coreset.id] = true;
      } else {
        logger.warning("Warning while building coreset structure");
        return false;
      }
    }
  } else {
    logger.warning("Option ctrl_res_set_to_add_mod_list not present");
  }
  return true;
}

static bool apply_csi_meas_cfg(srsran::phy_cfg_nr_t&                            phy_cfg,
                               asn1::rrc_nr::csi_meas_cfg_s&                    csi_meas_cfg,
                               std::map<uint32_t, srsran_csi_rs_nzp_resource_t> csi_rs_nzp_res,
                               srslog::basic_logger&                            logger)
{
  for (uint32_t i = 0; i < csi_meas_cfg.nzp_csi_rs_res_to_add_mod_list.size(); i++) {
    srsran_csi_rs_nzp_resource_t csi_rs_nzp_resource;
    if (srsran::make_phy_nzp_csi_rs_resource(csi_meas_cfg.nzp_csi_rs_res_to_add_mod_list[i], &csi_rs_nzp_resource) ==
        true) {
      csi_rs_nzp_res[csi_rs_nzp_resource.id] = csi_rs_nzp_resource;
    } else {
      logger.warning("Warning while building nzp_csi_rs resource");
      return false;
    }
  }

  for (uint32_t i = 0; i < csi_meas_cfg.nzp_csi_rs_res_set_to_add_mod_list.size(); i++) {
    uint8_t set_id = csi_meas_cfg.nzp_csi_rs_res_set_to_add_mod_list[i].nzp_csi_res_set_id;
    for (uint32_t j = 0; j < csi_meas_cfg.nzp_csi_rs_res_set_to_add_mod_list[i].nzp_csi_rs_res.size(); j++) {
      uint8_t res = csi_meas_cfg.nzp_csi_rs_res_set_to_add_mod_list[i].nzp_csi_rs_res[j];
      if (csi_rs_nzp_res.find(res) == csi_rs_nzp_res.end()) {
        logger.warning("Cannot find nzp_csi_rs_res in temporally stored csi_rs_nzp_res");
        return false;
      }
      phy_cfg.pdsch.nzp_csi_rs_sets[set_id].data[j] = csi_rs_nzp_res[res];
      phy_cfg.pdsch.nzp_csi_rs_sets[set_id].count += 1;
    }
    if (csi_meas_cfg.nzp_csi_rs_res_set_to_add_mod_list[i].trs_info_present) {
      phy_cfg.pdsch.nzp_csi_rs_sets[set_id].trs_info = true;
    }
  }
  return true;
}

/* pdsch configuration update from cell config */
static bool apply_sp_cell_init_dl_pdsch(srsran::phy_cfg_nr_t&                           phy_cfg,
                                        const asn1::rrc_nr::pdsch_cfg_s&                pdsch_cfg,
                                        std::map<uint32_t, srsran_csi_rs_zp_resource_t> csi_rs_zp_res,
                                        srslog::basic_logger&                           logger)
{
  if (pdsch_cfg.mcs_table_present) {
    switch (pdsch_cfg.mcs_table) {
      case asn1::rrc_nr::pdsch_cfg_s::mcs_table_opts::qam256:
        phy_cfg.pdsch.mcs_table = srsran_mcs_table_256qam;
        break;
      case asn1::rrc_nr::pdsch_cfg_s::mcs_table_opts::qam64_low_se:
        phy_cfg.pdsch.mcs_table = srsran_mcs_table_qam64LowSE;
        break;
      case asn1::rrc_nr::pdsch_cfg_s::mcs_table_opts::nulltype:
        logger.warning("Warning while selecting pdsch mcs_table");
        return false;
    }
  } else {
    // If the field is absent the UE applies the value 64QAM.
    phy_cfg.pdsch.mcs_table = srsran_mcs_table_64qam;
  }

  if (pdsch_cfg.dmrs_dl_for_pdsch_map_type_a_present) {
    if (pdsch_cfg.dmrs_dl_for_pdsch_map_type_a.type() ==
        asn1::setup_release_c<asn1::rrc_nr::dmrs_dl_cfg_s>::types_opts::setup) {
      // See TS 38.331, DMRS-DownlinkConfig. Also, see TS 38.214, 5.1.6.2 - DM-RS reception procedure.
      phy_cfg.pdsch.dmrs_typeA.additional_pos = srsran_dmrs_sch_add_pos_2;
      phy_cfg.pdsch.dmrs_typeA.present        = true;
    } else {
      logger.warning("Option dmrs_dl_for_pdsch_map_type_a not of type setup");
      return false;
    }
  } else {
    logger.warning("Option dmrs_dl_for_pdsch_map_type_a not present");
    return false;
  }

  srsran_resource_alloc_t resource_alloc;
  if (srsran::make_phy_pdsch_alloc_type(pdsch_cfg, &resource_alloc) == true) {
    phy_cfg.pdsch.alloc = resource_alloc;
  }
  if (pdsch_cfg.zp_csi_rs_res_to_add_mod_list.size() > 0) {
    for (uint32_t i = 0; i < pdsch_cfg.zp_csi_rs_res_to_add_mod_list.size(); i++) {
      srsran_csi_rs_zp_resource_t zp_csi_rs_resource;
      if (srsran::make_phy_zp_csi_rs_resource(pdsch_cfg.zp_csi_rs_res_to_add_mod_list[i], &zp_csi_rs_resource) ==
          true) {
        // temporally store csi_rs_zp_res
        csi_rs_zp_res[zp_csi_rs_resource.id] = zp_csi_rs_resource;
      } else {
        logger.warning("Warning while building zp_csi_rs resource");
        return false;
      }
    }
  }

  if (pdsch_cfg.p_zp_csi_rs_res_set_present) {
    // check if resources have been processed
    if (pdsch_cfg.zp_csi_rs_res_to_add_mod_list.size() == 0) {
      logger.warning("Can't build ZP-CSI config, option zp_csi_rs_res_to_add_mod_list not present");
      return false;
    }
    if (pdsch_cfg.p_zp_csi_rs_res_set.type() ==
        asn1::setup_release_c<asn1::rrc_nr::zp_csi_rs_res_set_s>::types_opts::setup) {
      for (uint32_t i = 0; i < pdsch_cfg.p_zp_csi_rs_res_set.setup().zp_csi_rs_res_id_list.size(); i++) {
        uint8_t res = pdsch_cfg.p_zp_csi_rs_res_set.setup().zp_csi_rs_res_id_list[i];
        // use temporally stored values to assign
        if (csi_rs_zp_res.find(res) == csi_rs_zp_res.end()) {
          logger.warning("Can not find p_zp_csi_rs_res in temporally stored csi_rs_zp_res");
          return false;
        }
        phy_cfg.pdsch.p_zp_csi_rs_set.data[i] = csi_rs_zp_res[res];
        phy_cfg.pdsch.p_zp_csi_rs_set.count += 1;
      }
    } else {
      logger.warning("Option p_zp_csi_rs_res_set not of type setup");
      return false;
    }
  }
  return true;
}

/* pucch configuration update from cell config */
static bool
apply_sp_cell_ded_ul_pucch(srsran::phy_cfg_nr_t&                                                    phy_cfg,
                           const asn1::rrc_nr::pucch_cfg_s&                                         pucch_cfg,
                           srsran::static_circular_map<uint32_t, srsran_pucch_nr_resource_t, 128UL> pucch_res_list,
                           srslog::basic_logger&                                                    logger)
{
  // determine format 2 max code rate
  uint32_t format_2_max_code_rate = 0;
  if (pucch_cfg.format2_present &&
      pucch_cfg.format2.type() == asn1::setup_release_c<asn1::rrc_nr::pucch_format_cfg_s>::types::setup) {
    if (pucch_cfg.format2.setup().max_code_rate_present) {
      if (srsran::make_phy_max_code_rate(pucch_cfg.format2.setup(), &format_2_max_code_rate) == false) {
        logger.warning("Warning while building format_2_max_code_rate");
      }
    }
  } else {
    logger.warning("Option format2 not present or not of type setup");
    return false;
  }

  // now look up resource and assign into internal struct
  if (pucch_cfg.res_to_add_mod_list.size() > 0) {
    for (uint32_t i = 0; i < pucch_cfg.res_to_add_mod_list.size(); i++) {
      uint32_t res_id = pucch_cfg.res_to_add_mod_list[i].pucch_res_id;
      pucch_res_list.insert(res_id, {});
      if (!srsran::make_phy_res_config(
              pucch_cfg.res_to_add_mod_list[i], format_2_max_code_rate, &pucch_res_list[res_id])) {
        logger.warning("Warning while building pucch_nr_resource structure");
        return false;
      }
    }
  } else {
    logger.warning("Option res_to_add_mod_list not present");
    return false;
  }

  // Check first all resource lists and
  phy_cfg.pucch.enabled = true;
  if (pucch_cfg.res_set_to_add_mod_list.size() > 0) {
    for (uint32_t i = 0; i < pucch_cfg.res_set_to_add_mod_list.size(); i++) {
      uint32_t set_id                          = pucch_cfg.res_set_to_add_mod_list[i].pucch_res_set_id;
      phy_cfg.pucch.sets[set_id].nof_resources = pucch_cfg.res_set_to_add_mod_list[i].res_list.size();
      for (uint32_t j = 0; j < pucch_cfg.res_set_to_add_mod_list[i].res_list.size(); j++) {
        uint32_t res_id = pucch_cfg.res_set_to_add_mod_list[i].res_list[j];
        if (pucch_res_list.contains(res_id)) {
          phy_cfg.pucch.sets[set_id].resources[j] = pucch_res_list[res_id];
        } else {
          logger.error(
              "Resources set not present for assign pucch sets (res_id %d, setid %d, j %d)", res_id, set_id, j);
        }
      }
    }
  }

  if (pucch_cfg.sched_request_res_to_add_mod_list.size() > 0) {
    for (uint32_t i = 0; i < pucch_cfg.sched_request_res_to_add_mod_list.size(); i++) {
      uint32_t                      sr_res_id = pucch_cfg.sched_request_res_to_add_mod_list[i].sched_request_res_id;
      srsran_pucch_nr_sr_resource_t srsran_pucch_nr_sr_resource;
      if (srsran::make_phy_sr_resource(pucch_cfg.sched_request_res_to_add_mod_list[i], &srsran_pucch_nr_sr_resource) ==
          true) { // TODO: fix that if indexing is solved
        phy_cfg.pucch.sr_resources[sr_res_id] = srsran_pucch_nr_sr_resource;

        // Set PUCCH resource
        if (pucch_cfg.sched_request_res_to_add_mod_list[i].res_present) {
          uint32_t pucch_res_id = pucch_cfg.sched_request_res_to_add_mod_list[i].res;
          if (pucch_res_list.contains(pucch_res_id)) {
            phy_cfg.pucch.sr_resources[sr_res_id].resource = pucch_res_list[pucch_res_id];
          } else {
            logger.warning("Warning SR (%d) PUCCH resource is invalid (%d)", sr_res_id, pucch_res_id);
            phy_cfg.pucch.sr_resources[sr_res_id].configured = false;
            return false;
          }
        } else {
          logger.warning("Warning SR resource is present but no PUCCH resource is assigned to it");
          phy_cfg.pucch.sr_resources[sr_res_id].configured = false;
          return false;
        }

      } else {
        logger.warning("Warning while building srsran_pucch_nr_sr_resource structure");
        return false;
      }
    }
  } else {
    logger.warning("Option sched_request_res_to_add_mod_list not present");
    return false;
  }

  if (pucch_cfg.dl_data_to_ul_ack.size() > 0) {
    for (uint32_t i = 0; i < pucch_cfg.dl_data_to_ul_ack.size(); i++) {
      phy_cfg.harq_ack.dl_data_to_ul_ack[i] = pucch_cfg.dl_data_to_ul_ack[i];
    }
    phy_cfg.harq_ack.nof_dl_data_to_ul_ack = pucch_cfg.dl_data_to_ul_ack.size();
  } else {
    logger.warning("Option dl_data_to_ul_ack not present");
    return false;
  }

  return true;
};

/* pusch configuration update from cell config */
static bool apply_sp_cell_ded_ul_pusch(srsran::phy_cfg_nr_t&            phy_cfg,
                                       const asn1::rrc_nr::pusch_cfg_s& pusch_cfg,
                                       srslog::basic_logger&            logger)
{
  if (pusch_cfg.mcs_table_present) {
    switch (pusch_cfg.mcs_table) {
      case asn1::rrc_nr::pusch_cfg_s::mcs_table_opts::qam256:
        phy_cfg.pusch.mcs_table = srsran_mcs_table_256qam;
        break;
      case asn1::rrc_nr::pusch_cfg_s::mcs_table_opts::qam64_low_se:
        phy_cfg.pusch.mcs_table = srsran_mcs_table_qam64LowSE;
        break;
      case asn1::rrc_nr::pusch_cfg_s::mcs_table_opts::nulltype:
        logger.warning("Warning while selecting pusch mcs_table");
        return false;
    }
  } else {
    // If the field is absent the UE applies the value 64QAM.
    phy_cfg.pusch.mcs_table = srsran_mcs_table_64qam;
  }

  srsran_resource_alloc_t resource_alloc;
  if (srsran::make_phy_pusch_alloc_type(pusch_cfg, &resource_alloc) == true) {
    phy_cfg.pusch.alloc = resource_alloc;
  }

  if (pusch_cfg.dmrs_ul_for_pusch_map_type_a_present) {
    if (pusch_cfg.dmrs_ul_for_pusch_map_type_a.type() ==
        asn1::setup_release_c<asn1::rrc_nr::dmrs_ul_cfg_s>::types_opts::setup) {
      // See TS 38.331, DMRS-UplinkConfig. Also, see TS 38.214, 6.2.2 - UE DM-RS transmission procedure.
      phy_cfg.pusch.dmrs_typeA.additional_pos = srsran_dmrs_sch_add_pos_2;
      phy_cfg.pusch.dmrs_typeA.present        = true;
    } else {
      logger.warning("Option dmrs_ul_for_pusch_map_type_a not of type setup");
      return false;
    }
  } else {
    logger.warning("Option dmrs_ul_for_pusch_map_type_a not present");
    return false;
  }
  if (pusch_cfg.uci_on_pusch_present) {
    if (pusch_cfg.uci_on_pusch.type() == asn1::setup_release_c<asn1::rrc_nr::uci_on_pusch_s>::types_opts::setup) {
      if (pusch_cfg.uci_on_pusch.setup().beta_offsets_present) {
        if (pusch_cfg.uci_on_pusch.setup().beta_offsets.type() ==
            asn1::rrc_nr::uci_on_pusch_s::beta_offsets_c_::types_opts::semi_static) {
          srsran_beta_offsets_t beta_offsets;
          if (srsran::make_phy_beta_offsets(pusch_cfg.uci_on_pusch.setup().beta_offsets.semi_static(), &beta_offsets) ==
              true) {
            phy_cfg.pusch.beta_offsets = beta_offsets;
          } else {
            logger.warning("Warning while building beta_offsets structure");
            return false;
          }
        } else {
          logger.warning("Option beta_offsets not of type semi_static");
          return false;
        }
        if (pusch_cfg.uci_on_pusch_present) {
          if (srsran::make_phy_pusch_scaling(pusch_cfg.uci_on_pusch.setup(), &phy_cfg.pusch.scaling) == false) {
            logger.warning("Warning while building scaling structure");
            return false;
          }
        }
      } else {
        logger.warning("Option beta_offsets not present");
        return false;
      }
    } else {
      logger.warning("Option uci_on_pusch of type setup");
      return false;
    }
  } else {
    logger.warning("Option uci_on_pusch not present");
    return false;
  }
  return true;
};

/* Apply cell configuration to phy cfg, Copy from file rrc_nr.cc */
bool update_phy_cfg_from_cell_cfg(srsran::phy_cfg_nr_t&                                                  phy_cfg,
                                  asn1::rrc_nr::sp_cell_cfg_s&                                           sp_cell_cfg,
                                  srsran::static_circular_map<uint32_t, srsran_pucch_nr_resource_t, 128> pucch_res_list,
                                  std::map<uint32_t, srsran_csi_rs_zp_resource_t>                        csi_rs_zp_res,
                                  std::map<uint32_t, srsran_csi_rs_nzp_resource_t>                       csi_rs_nzp_res,
                                  srslog::basic_logger&                                                  logger)
{
  // NSA specific handling to defer CSI, SR, SRS config until after RA (see TS 38.331, Section 5.3.5.3)
  srsran_csi_hl_cfg_t prev_csi = phy_cfg.csi;
  // Dedicated config
  if (sp_cell_cfg.sp_cell_cfg_ded_present) {
    // Dedicated Downlink
    if (sp_cell_cfg.sp_cell_cfg_ded.init_dl_bwp_present) {
      if (sp_cell_cfg.sp_cell_cfg_ded.init_dl_bwp.pdcch_cfg_present) {
        if (sp_cell_cfg.sp_cell_cfg_ded.init_dl_bwp.pdcch_cfg.type() ==
            asn1::setup_release_c<asn1::rrc_nr::pdcch_cfg_s>::types_opts::setup) {
          if (apply_sp_cell_init_dl_pdcch(phy_cfg, sp_cell_cfg.sp_cell_cfg_ded.init_dl_bwp.pdcch_cfg.setup(), logger) ==
              false) {
            return false;
          }
        } else {
          logger.warning("Option pdcch_cfg not of type setup");
          return false;
        }
      } else {
        logger.warning("Option pdcch_cfg not present");
        return false;
      }
      if (sp_cell_cfg.sp_cell_cfg_ded.init_dl_bwp.pdsch_cfg_present) {
        if (sp_cell_cfg.sp_cell_cfg_ded.init_dl_bwp.pdsch_cfg.type() ==
            asn1::setup_release_c<asn1::rrc_nr::pdsch_cfg_s>::types_opts::setup) {
          if (apply_sp_cell_init_dl_pdsch(
                  phy_cfg, sp_cell_cfg.sp_cell_cfg_ded.init_dl_bwp.pdsch_cfg.setup(), csi_rs_zp_res, logger) == false) {
            logger.error("Couldn't apply PDSCH config for initial DL BWP in SpCell Cfg dedicated");
            return false;
          };
        } else {
          logger.warning("Option pdsch_cfg_cfg not of type setup");
          return false;
        }
      } else {
        logger.warning("Option pdsch_cfg not present");
        return false;
      }
    } else {
      logger.warning("Option init_dl_bwp not present");
      return false;
    }
    if (sp_cell_cfg.sp_cell_cfg_ded.csi_meas_cfg_present) {
      if (!apply_csi_meas_cfg(phy_cfg, sp_cell_cfg.sp_cell_cfg_ded.csi_meas_cfg.setup(), csi_rs_nzp_res, logger)) {
        logger.error("Couldn't apply csi_meas_cfg to phy_cfg");
        return false;
      }
    }
    // Dedicated Uplink
    if (sp_cell_cfg.sp_cell_cfg_ded.ul_cfg_present) {
      if (sp_cell_cfg.sp_cell_cfg_ded.ul_cfg.init_ul_bwp_present) {
        if (sp_cell_cfg.sp_cell_cfg_ded.ul_cfg.init_ul_bwp.pucch_cfg_present) {
          if (sp_cell_cfg.sp_cell_cfg_ded.ul_cfg.init_ul_bwp.pucch_cfg.type() ==
              asn1::setup_release_c<asn1::rrc_nr::pucch_cfg_s>::types_opts::setup) {
            if (apply_sp_cell_ded_ul_pucch(phy_cfg,
                                           sp_cell_cfg.sp_cell_cfg_ded.ul_cfg.init_ul_bwp.pucch_cfg.setup(),
                                           pucch_res_list,
                                           logger) == false) {
              return false;
            }
          } else {
            logger.warning("Option pucch_cfg not of type setup");
            return false;
          }
        } else {
          logger.warning("Option pucch_cfg for initial UL BWP in spCellConfigDedicated not present");
          return false;
        }
        if (sp_cell_cfg.sp_cell_cfg_ded.ul_cfg.init_ul_bwp.pusch_cfg_present) {
          if (sp_cell_cfg.sp_cell_cfg_ded.ul_cfg.init_ul_bwp.pusch_cfg.type() ==
              asn1::setup_release_c<asn1::rrc_nr::pusch_cfg_s>::types_opts::setup) {
            if (apply_sp_cell_ded_ul_pusch(
                    phy_cfg, sp_cell_cfg.sp_cell_cfg_ded.ul_cfg.init_ul_bwp.pusch_cfg.setup(), logger) == false) {
              return false;
            }
          } else {
            logger.warning("Option pusch_cfg not of type setup");
            return false;
          }
        } else {
          logger.warning("Option pusch_cfg in spCellConfigDedicated not present");
          return false;
        }
      } else {
        logger.warning("Option init_ul_bwp in spCellConfigDedicated not present");
        return false;
      }
    } else {
      logger.warning("Option ul_cfg in spCellConfigDedicated not present");
      return false;
    }
  } else {
    logger.warning("Option sp_cell_cfg_ded not present");
    return false;
  }

  if (sp_cell_cfg.recfg_with_sync_present) {
    phy_cfg.csi = prev_csi;
  }
  return true;
}

/* Decode dl_ccch_msg_s bytes to asn1 structure */
bool parse_to_dl_ccch_msg(uint8_t* data, uint32_t len, asn1::rrc_nr::dl_ccch_msg_s& dl_ccch_msg)
{
  asn1::cbit_ref    bref(data, len);
  asn1::SRSASN_CODE err = dl_ccch_msg.unpack(bref);
  if (err != asn1::SRSASN_SUCCESS || dl_ccch_msg.msg.type().value != asn1::rrc_nr::dl_ccch_msg_type_c::types_opts::c1) {
    std::cerr << "Error unpacking DL-CCCH message\n";
    return false;
  }
  return true;
}

/* extract cell_group struct from rrc_setup */
bool extract_cell_group_cfg(asn1::rrc_nr::dl_ccch_msg_s& dl_ccch_msg, asn1::rrc_nr::cell_group_cfg_s& cell_group)
{
  asn1::rrc_nr::rrc_setup_s& rrc_setup_msg = dl_ccch_msg.msg.c1().rrc_setup();
  asn1::cbit_ref             bref_cg(rrc_setup_msg.crit_exts.rrc_setup().master_cell_group.data(),
                         rrc_setup_msg.crit_exts.rrc_setup().master_cell_group.size());
  if (cell_group.unpack(bref_cg) != asn1::SRSASN_SUCCESS) {
    printf("Could not unpack master cell group config.\n");
    return false;
  }
  return true;
}