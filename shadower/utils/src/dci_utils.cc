#include "shadower/utils/dci_utils.h"

/* Construct the dci to send */
bool construct_dci_dl_to_send(srsran_dci_dl_nr_t&   dci_to_send,
                              srsran::phy_cfg_nr_t& phy_cfg,
                              uint32_t              slot_idx,
                              uint16_t              rnti,
                              srsran_rnti_type_t    rnti_type,
                              uint32_t              mcs,
                              uint32_t              nof_prb_to_allocate)
{
  /* find a search space to use */
  srsran_search_space_t* search_space = nullptr;
  if (!find_search_space(&search_space, phy_cfg, srsran_dci_format_nr_1_0)) {
    return false;
  }
  /* get the coreset corresponding to the search space */
  srsran_coreset_t* coreset = &phy_cfg.pdcch.coreset[search_space->coreset_id];
  /* Initialize start resource block */
  uint32_t start_rb = 0;
  if (SRSRAN_SEARCH_SPACE_IS_COMMON(search_space->type)) {
    start_rb = coreset->offset_rb;
  }
  /* Get bwp size */
  uint32_t coreset_bw   = srsran_coreset_get_bw(coreset);
  uint32_t type1_bwp_sz = phy_cfg.carrier.nof_prb;
  if (SRSRAN_SEARCH_SPACE_IS_COMMON(search_space->type) && coreset_bw != 0) {
    type1_bwp_sz = coreset_bw;
  }

  dci_to_send.mcs     = mcs;
  nof_prb_to_allocate = std::min(nof_prb_to_allocate, coreset_bw);
  if (SRSRAN_SEARCH_SPACE_IS_COMMON(search_space->type) && coreset_bw != 0) {
    type1_bwp_sz = coreset_bw;
  }
  uint32_t freq_domain_assignment    = srsran_ra_nr_type1_riv(type1_bwp_sz, 0, nof_prb_to_allocate); /* RIV */
  dci_to_send.freq_domain_assignment = freq_domain_assignment;
  dci_to_send.time_domain_assignment = 0;
  dci_to_send.coreset0_bw            = coreset_bw;
  dci_to_send.ctx.coreset_id         = search_space->coreset_id;
  dci_to_send.ctx.coreset_start_rb   = coreset->offset_rb;
  dci_to_send.ctx.ss_type            = search_space->type;
  dci_to_send.ctx.rnti_type          = rnti_type;
  dci_to_send.ctx.rnti               = rnti;
  dci_to_send.ctx.format             = srsran_dci_format_nr_1_0;
  if (!find_aggregation_level(dci_to_send.ctx, coreset, search_space, slot_idx, rnti)) {
    return false;
  }
  return true;
}

bool construct_dci_ul_to_send(srsran_dci_ul_nr_t&   dci_to_send,
                              srsran::phy_cfg_nr_t& phy_cfg,
                              uint32_t              slot_idx,
                              uint16_t              rnti,
                              srsran_rnti_type_t    rnti_type,
                              uint32_t              mcs,
                              uint32_t              nof_prb_to_allocate)
{
  /* find a search space to use */
  srsran_search_space_t* search_space = nullptr;
  if (!find_search_space(&search_space, phy_cfg, srsran_dci_format_nr_0_0)) {
    return false;
  }
  /* get the coreset corresponding to the search space */
  srsran_coreset_t* coreset = &phy_cfg.pdcch.coreset[search_space->coreset_id];
  dci_to_send.mcs           = mcs;
  uint32_t coreset_bw       = srsran_coreset_get_bw(coreset);
  uint32_t freq_domain_assignment =
      srsran_ra_type2_to_riv(nof_prb_to_allocate, coreset->offset_rb, coreset_bw); /* RIV */
  dci_to_send.freq_domain_assignment = freq_domain_assignment;
  dci_to_send.time_domain_assignment = 0;
  dci_to_send.ndi                    = 1;
  dci_to_send.tpc                    = 1;
  dci_to_send.ctx.coreset_id         = coreset->id;
  dci_to_send.ctx.coreset_start_rb   = coreset->offset_rb;
  dci_to_send.ctx.ss_type            = search_space->type;
  dci_to_send.ctx.rnti_type          = rnti_type;
  dci_to_send.ctx.rnti               = rnti;
  dci_to_send.ctx.format             = srsran_dci_format_nr_0_0;
  if (!find_aggregation_level(dci_to_send.ctx, coreset, search_space, slot_idx, rnti)) {
    return false;
  }
  return true;
}

/* Find a search space that contains target dci format */
bool find_search_space(srsran_search_space_t** search_space,
                       srsran::phy_cfg_nr_t&   phy_cfg,
                       srsran_dci_format_nr_t  format)
{
  for (uint32_t i = 1; i < SRSRAN_UE_DL_NR_MAX_NOF_SEARCH_SPACE; i++) {
    if (!phy_cfg.pdcch.search_space_present[i]) {
      continue;
    }
    srsran_search_space_t* current_search_space = &phy_cfg.pdcch.search_space[i];
    for (uint32_t j = 0; j < current_search_space->nof_formats; j++) {
      if (current_search_space->formats[j] == format) {
        *search_space = current_search_space;
        return true;
      }
    }
  }
  if (!phy_cfg.pdcch.search_space_present[0]) {
    return false;
  }
  srsran_search_space_t* search_space0 = &phy_cfg.pdcch.search_space[0];
  for (uint32_t j = 0; j < search_space0->nof_formats; j++) {
    if (search_space0->formats[j] == format) {
      *search_space = search_space0;
      return true;
    }
  }
  return false;
}

/* Find an aggregation level to use */
bool find_aggregation_level(srsran_dci_ctx_t&      dci_ctx,
                            srsran_coreset_t*      coreset,
                            srsran_search_space_t* search_space,
                            uint32_t               slot_idx,
                            uint16_t               rnti)
{
  for (uint32_t agl = 0; agl < SRSRAN_SEARCH_SPACE_NOF_AGGREGATION_LEVELS_NR; agl++) {
    uint32_t L                                                        = 1U << agl;
    uint32_t dci_locations[SRSRAN_SEARCH_SPACE_MAX_NOF_CANDIDATES_NR] = {};
    int      n = srsran_pdcch_nr_locations_coreset(coreset, search_space, rnti, agl, slot_idx, dci_locations);
    if (n < SRSRAN_SUCCESS) {
      return false;
    }
    if (n == 0) {
      continue;
    }
    const uint32_t nof_locations = static_cast<uint32_t>(n);
    for (uint32_t ncce_idx = 0; ncce_idx < nof_locations; ncce_idx++) {
      dci_ctx.location.L    = agl;
      dci_ctx.location.ncce = dci_locations[ncce_idx];
      return true;
    }
  }
  return false;
}