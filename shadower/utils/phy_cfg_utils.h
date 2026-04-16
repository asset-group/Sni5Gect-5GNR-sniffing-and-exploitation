#pragma once
#include "shadower/utils/arg_parser.h"
#include "shadower/utils/utils.h"
#include "srsran/adt/circular_map.h"
#include "srsran/asn1/rrc_nr/bcch_dl_sch_msg.h"
#include "srsran/common/phy_cfg_nr.h"
#include "srsue/hdr/phy/nr/state.h"

/* Initialize phy cfg from shadower configuration */
void init_phy_cfg(srsran::phy_cfg_nr_t& phy_cfg, ShadowerConfig& config);

/* Initialize phy state object */
void init_phy_state(srsue::nr::state& phy_state, uint32_t nof_prb);

/* Decode SIB1 bytes to asn1 structure */
bool parse_to_sib1(uint8_t* data, uint32_t len, asn1::rrc_nr_r17::sib1_s& sib1);

/* Set rar grant */
bool set_rar_grant(uint16_t                                        rnti,
                   srsran_rnti_type_t                              rnti_type,
                   uint32_t                                        slot_idx,
                   std::array<uint8_t, SRSRAN_RAR_UL_GRANT_NBITS>& grant,
                   srsran::phy_cfg_nr_t&                           phy_cfg,
                   srsue::nr::state&                               phy_state,
                   uint32_t*                                       grant_k,
                   srslog::basic_logger&                           logger);

/* Load mib configuration from file and apply to phy cfg */
bool configure_phy_cfg_from_mib(srsran::phy_cfg_nr_t& phy_cfg, std::string& filename, uint32_t ncellid);

/* Load SIB1 configuration from file and apply to phy cfg */
bool configure_phy_cfg_from_sib1(srsran::phy_cfg_nr_t& phy_cfg, std::string& filename, uint32_t nbits);

/* Load RRC setup cell configuration from file and apply to phy cfg */
bool configure_phy_cfg_from_rrc_setup(srsran::phy_cfg_nr_t& phy_cfg,
                                      std::string&          filename,
                                      uint32_t              nbits,
                                      srslog::basic_logger& logger);

/* Load rrc reconfiguration from file and apply to phy cfg */
bool configure_phy_cfg_from_cell_group_cfg(srsran::phy_cfg_nr_t& phy_cfg, std::string& cell_group_cfg);
/* Apply MIB configuration to phy cfg */
bool update_phy_cfg_from_mib(srsran::phy_cfg_nr_t& phy_cfg, srsran_mib_nr_t& mib, uint32_t ncellid);

/* Apply SIB1 configuration to phy cfg */
void update_phy_cfg_from_sib1(srsran::phy_cfg_nr_t& phy_cfg, asn1::rrc_nr_r17::sib1_s& sib1);

/* Apply cell configuration to phy cfg */
bool update_phy_cfg_from_cell_cfg(srsran::phy_cfg_nr_t& phy_cfg, asn1::rrc_nr_r17::sp_cell_cfg_s& sp_cell_cfg);

/* Decode dl_ccch_msg_s bytes to asn1 structure */
bool parse_to_dl_ccch_msg(uint8_t* data, uint32_t len, asn1::rrc_nr_r17::dl_ccch_msg_s& dl_ccch_msg);

/* Decode dl_dcch_msg_s bytes to asn1 structure */
bool parse_to_dl_dcch_msg(uint8_t* data, uint32_t len, asn1::rrc_nr_r17::dl_dcch_msg_s& dl_dcch_msg);

/* extract cell_group struct from rrc_setup */
bool extract_cell_group_cfg(asn1::rrc_nr_r17::dl_ccch_msg_s&    dl_ccch_msg,
                            asn1::rrc_nr_r17::cell_group_cfg_s& cell_group);

bool extract_cell_group_cfg(asn1::rrc_nr_r17::rrc_recfg_s& rrc_recfg, asn1::rrc_nr_r17::cell_group_cfg_s& cell_group);