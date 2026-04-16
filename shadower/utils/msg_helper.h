#ifndef MSG_HELPER_H
#define MSG_HELPER_H
#include "srsran/asn1/rrc_nr/cell_group_config.h"
#include "srsran/asn1/rrc_nr/dl_ccch_msg.h"
#include "srsran/asn1/rrc_nr/dl_dcch_msg.h"
#include "srsran/asn1/rrc_nr/ul_ccch_msg.h"
#include "srsran/common/byte_buffer.h"
#include "srsran/mac/mac_sch_pdu_nr.h"
#include <string>

/* Put the nas message in to dl_dcch_msg */
asn1::rrc_nr_r17::dl_dcch_msg_s pack_nas_to_dl_dcch(const std::string& nas_msg);

/* Put the dl_dcch msg into rrc nr and encode it */
bool pack_dl_dcch_to_rrc_nr(srsran::unique_byte_buffer_t& buffer, const asn1::rrc_nr_r17::dl_dcch_msg_s& dl_dcch_msg);

void pack_rrc_nr_to_rlc_nr(uint8_t*                      rrc_nr_msg,
                           uint32_t                      rrc_nr_len,
                           uint16_t                      am_sn,
                           uint16_t                      pdcp_sn,
                           uint8_t*                      rrc_mac,
                           srsran::unique_byte_buffer_t& output);

void pack_rlc_nr_to_mac_nr(uint8_t*               rlc_nr_msg,
                           uint32_t               rlc_nr_len,
                           uint16_t               ack_sn,
                           srsran::byte_buffer_t& output,
                           uint32_t               pdu_len = 256);

/* Extract the contention resolution identity */
bool extract_con_res_id(const uint8_t*                              buffer,
                        const uint32_t                              len,
                        srsran::mac_sch_subpdu_nr::ue_con_res_id_t& con_res_id,
                        srslog::basic_logger&                       logger);

/* Since the RRC setup contains the contention resolution identity in SRSRAN,we have to replace the contentional
 * resolution id to the one used by UE in RRC setup request */
bool replace_con_res_id(srsran::mac_sch_pdu_nr                      original_rrc_setup,
                        const uint32_t                              origin_len,
                        srsran::mac_sch_subpdu_nr::ue_con_res_id_t& con_res_id,
                        srsran::byte_buffer_t&                      tx_buffer,
                        srslog::basic_logger&                       logger,
                        std::vector<uint8_t>*                       modified_dl_ccch_msg = nullptr);

bool modify_monitoring_symbol_within_slot(uint8_t*              dl_ccch_msg_raw,
                                          uint32_t              dl_ccch_msg_len,
                                          const std::string     symbol,
                                          std::vector<uint8_t>& modified_dl_ccch_msg);
#endif // MSG_HELPER_H