#include "srsran/adt/circular_map.h"
#include "srsran/asn1/rrc_nr/bwp_cfg.h"
#include "srsran/asn1/rrc_nr/serving_cell.h"
#include "srsran/common/phy_cfg_nr.h"
#include "srsran/phy/phch/phch_cfg_nr.h"
#include "srsran/phy/sync/ssb.h"

bool make_phy_common_time_ra(const asn1::rrc_nr_r17::pdsch_time_domain_res_alloc_s& time_domain_alloc,
                             srsran_sch_time_ra_t*                                  time_ra);

bool make_phy_common_time_ra(const asn1::rrc_nr_r17::pusch_time_domain_res_alloc_s& time_domain_alloc,
                             srsran_sch_time_ra_t*                                  time_ra);

bool fill_pdsch_cfg_common(asn1::rrc_nr_r17::pdsch_cfg_common_s& pdsch_cfg, srsran_sch_hl_cfg_nr_t* pdsch);

bool fill_pusch_cfg_common(asn1::rrc_nr_r17::pusch_cfg_common_s& pusch_cfg, srsran_sch_hl_cfg_nr_t* pusch);

bool make_phy_tdd_cfg(const asn1::rrc_nr_r17::tdd_ul_dl_cfg_common_s& tdd_ul_dl_cfg_common,
                      srsran_duplex_config_nr_t*                      in_srsran_duplex_config_nr);

void fill_phy_ssb_cfg(const asn1::rrc_nr_r17::serving_cell_cfg_common_sib_s& serv_cell_cfg,
                      srsran::phy_cfg_nr_t::ssb_cfg_t*                       out_ssb);

bool fill_phy_pusch_cfg_common(const asn1::rrc_nr_r17::pusch_cfg_common_s& pusch_cfg, srsran_sch_hl_cfg_nr_t* pusch);

void fill_phy_pucch_cfg_common(const asn1::rrc_nr_r17::pucch_cfg_common_s& pucch_cfg,
                               srsran_pucch_nr_common_cfg_t*               pucch);

bool make_phy_rach_cfg(const asn1::rrc_nr_r17::rach_cfg_common_s& asn1_type,
                       srsran_duplex_mode_t                       duplex_mode,
                       srsran_prach_cfg_t*                        prach_cfg);

void fill_phy_carrier_cfg(const asn1::rrc_nr_r17::serving_cell_cfg_common_sib_s& serv_cell_cfg,
                          srsran_carrier_nr_t*                                   out_carrier_nr);

bool fill_phy_pdsch_cfg_common(const asn1::rrc_nr_r17::pdsch_cfg_common_s& pdsch_cfg, srsran_sch_hl_cfg_nr_t* pdsch);

bool fill_phy_pdcch_cfg_common(const asn1::rrc_nr_r17::pdcch_cfg_common_s& pdcch_cfg, srsran_pdcch_cfg_nr_t* pdcch);

bool make_phy_coreset_cfg(const asn1::rrc_nr_r17::coreset_s& coreset, srsran_coreset_t* in_srsran_coreset);

bool make_phy_search_space_cfg(const asn1::rrc_nr_r17::search_space_s& search_space,
                               srsran_search_space_t*                  in_srsran_search_space);

bool make_phy_coreset_cfg(const asn1::rrc_nr_r17::coreset_s& coreset, srsran_coreset_t* in_srsran_coreset);

bool make_phy_search_space_cfg(const asn1::rrc_nr_r17::search_space_s& search_space,
                               srsran_search_space_t*                  in_srsran_search_space);

bool make_phy_pdsch_alloc_type(const asn1::rrc_nr_r17::pdsch_cfg_s& pdsch_cfg,
                               srsran_resource_alloc_t*             in_srsran_resource_alloc);

bool make_phy_dmrs_dl_additional_pos(const asn1::rrc_nr_r17::dmrs_dl_cfg_s& dmrs_dl_cfg,
                                     srsran_dmrs_sch_add_pos_t*             in_srsran_dmrs_sch_add_pos);

bool make_phy_dmrs_ul_additional_pos(const asn1::rrc_nr_r17::dmrs_ul_cfg_s& dmrs_ul_cfg,
                                     srsran_dmrs_sch_add_pos_t*             in_srsran_dmrs_sch_add_pos);

bool make_phy_zp_csi_rs_resource(const asn1::rrc_nr_r17::zp_csi_rs_res_s& zp_csi_rs_res,
                                 srsran_csi_rs_zp_resource_t*             out_zp_csi_rs_resource);

bool make_phy_nzp_csi_rs_resource(const asn1::rrc_nr_r17::nzp_csi_rs_res_s& asn1_nzp_csi_rs_res,
                                  srsran_csi_rs_nzp_resource_t*             out_csi_rs_nzp_resource);

/* Apply pucch_cfg in BWP-UplinkDedicated */
bool apply_dedicated_pucch_cfg(srsran::phy_cfg_nr_t&                phy_cfg,
                               const asn1::rrc_nr_r17::pucch_cfg_s& dedicated_pucch_cfg,
                               srsran::static_circular_map<uint32_t, srsran_pucch_nr_resource_t, 128UL> pucch_res_list);

bool make_phy_res_config(const asn1::rrc_nr_r17::pucch_res_s& pucch_res,
                         uint32_t                             format_2_max_code_rate,
                         srsran_pucch_nr_resource_t*          in_srsran_pucch_nr_resource);

bool make_phy_sr_resource(const asn1::rrc_nr_r17::sched_request_res_cfg_s& sched_request_res_cfg,
                          srsran_pucch_nr_sr_resource_t*                   in_srsran_pucch_nr_sr_resource);

bool make_phy_pusch_alloc_type(const asn1::rrc_nr_r17::pusch_cfg_s& pusch_cfg,
                               srsran_resource_alloc_t*             in_srsran_resource_alloc);

bool make_phy_pdsch_alloc_type(const asn1::rrc_nr_r17::pdsch_cfg_s& pdsch_cfg,
                               srsran_resource_alloc_t*             in_srsran_resource_alloc);

bool make_phy_beta_offsets(const asn1::rrc_nr_r17::beta_offsets_s& beta_offsets,
                           srsran_beta_offsets_t*                  in_srsran_beta_offsets);

bool make_phy_pusch_scaling(const asn1::rrc_nr_r17::uci_on_pusch_s& uci_on_pusch, float* in_scaling);