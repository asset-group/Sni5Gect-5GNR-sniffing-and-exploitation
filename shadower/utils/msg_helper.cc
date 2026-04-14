#include "shadower/utils/msg_helper.h"
#include "shadower/utils/phy_cfg_utils.h"
#include "srsran/asn1/rrc_nr/rrc_nr.h"
#include "srsran/mac/mac_sch_pdu_nr.h"
#include <iomanip>
#include <iostream>
#include <sstream>

/* Put the nas message in to dl_dcch_msg */
asn1::rrc_nr_r17::dl_dcch_msg_s pack_nas_to_dl_dcch(const std::string& nas_msg)
{
  asn1::rrc_nr_r17::dl_dcch_msg_s       dl_dcch_msg;
  asn1::rrc_nr_r17::dl_info_transfer_s& dl_information_transfer = dl_dcch_msg.msg.set_c1().set_dl_info_transfer();
  dl_information_transfer.rrc_transaction_id                    = 0;
  asn1::rrc_nr_r17::dl_info_transfer_ies_s& dl_information_transfer_ies =
      dl_information_transfer.crit_exts.set_dl_info_transfer();
  dl_information_transfer_ies.ded_nas_msg.from_string(nas_msg);
  return dl_dcch_msg;
}

/* Put the dl_dcch msg into rrc nr and encode it */
bool pack_dl_dcch_to_rrc_nr(srsran::unique_byte_buffer_t& buffer, const asn1::rrc_nr_r17::dl_dcch_msg_s& dl_dcch_msg)
{
  asn1::bit_ref bref{buffer->msg + buffer->N_bytes, buffer->get_tailroom()};
  if (dl_dcch_msg.pack(bref) != asn1::SRSASN_SUCCESS) {
    printf("Error packing dl_info_transfer\n");
    return false;
  }
  buffer->N_bytes += bref.distance_bytes();
  return true;
}

void pack_rrc_nr_to_rlc_nr(uint8_t*                      rrc_nr_msg,
                           uint32_t                      rrc_nr_len,
                           uint16_t                      am_sn,
                           uint16_t                      pdcp_sn,
                           uint8_t*                      rrc_mac,
                           srsran::unique_byte_buffer_t& output)
{
  /* AM header */
  uint8_t am_header[2] = {0};
  am_header[0]         = ((am_sn >> 8) & 0xf) | 0xc0;
  am_header[1]         = am_sn & 0xff;
  output->append_bytes(am_header, sizeof(am_header));

  /* PDCP header */
  uint8_t pdcp_header[2] = {0};
  pdcp_header[0]         = (pdcp_sn >> 8) & 0xff;
  pdcp_header[1]         = pdcp_sn & 0xff;
  output->append_bytes(pdcp_header, sizeof(pdcp_header));

  /* Put the rrc-nr message into rlc-nr */
  output->append_bytes(rrc_nr_msg, rrc_nr_len);

  /* Append the rrc-nr mac to the end */
  output->append_bytes(rrc_mac, 4);
}

/* Put rrc nr message into rlc nr*/
void pack_rlc_nr_to_mac_nr(uint8_t*               rlc_nr_msg,
                           uint32_t               rlc_nr_len,
                           uint16_t               ack_sn,
                           srsran::byte_buffer_t& output,
                           uint32_t               pdu_len)
{
  srsran::mac_sch_pdu_nr mac_pdu;
  mac_pdu.init_tx(&output, pdu_len);

  if (ack_sn > 0) {
    /* add an ack to the message */
    uint8_t rlc_ack_pdu[3] = {0};
    rlc_ack_pdu[0]         = 0x0;
    rlc_ack_pdu[1]         = (ack_sn >> 8) & 0xf;
    rlc_ack_pdu[2]         = ack_sn & 0xff;
    mac_pdu.add_sdu(1, rlc_ack_pdu, 3);
  }

  /* Add the rlc-nr buffer to mac-nr*/
  mac_pdu.add_sdu(1, rlc_nr_msg, rlc_nr_len);
  mac_pdu.pack();
}

/* Extract the contention resolution identity */
bool extract_con_res_id(const uint8_t*                              buffer,
                        const uint32_t                              len,
                        srsran::mac_sch_subpdu_nr::ue_con_res_id_t& con_res_id,
                        srslog::basic_logger&                       logger)
{
  /* Unpack mac_sch */
  srsran::mac_sch_pdu_nr pdu(true);
  if (pdu.unpack(buffer, len) != SRSRAN_SUCCESS) {
    logger.error("Failed to unpack MAC SDU");
    return false;
  }
  for (uint32_t i = 0; i < pdu.get_num_subpdus(); i++) {
    srsran::mac_sch_subpdu_nr& subpdu = pdu.get_subpdu(i);
    if (subpdu.get_lcid() == srsran::mac_sch_subpdu_nr::nr_lcid_sch_t::CCCH_SIZE_48) {
      /* Unpack to ul_ccch_msg */
      asn1::rrc_nr_r17::ul_ccch_msg_s ul_ccch_msg;
      asn1::cbit_ref                  bref(subpdu.get_sdu(), subpdu.get_sdu_length());
      asn1::SRSASN_CODE               err = ul_ccch_msg.unpack(bref);
      if (err != asn1::SRSASN_SUCCESS ||
          ul_ccch_msg.msg.type().value != asn1::rrc_nr_r17::ul_ccch_msg_type_c::types_opts::c1) {
        logger.error("Error unpacking UL-CCCH message");
        return false;
      }

      /* Check if it is RRC setup request */
      if (ul_ccch_msg.msg.c1().type().value != asn1::rrc_nr_r17::ul_ccch_msg_type_c::c1_c_::types::rrc_setup_request) {
        logger.error("Not RRC setup request");
        return false;
      }

      /* Unpack and extract ue contention resolution identity */
      asn1::rrc_nr_r17::rrc_setup_request_s& rrc_setup_req        = ul_ccch_msg.msg.c1().rrc_setup_request();
      asn1::rrc_nr_r17::init_ue_id_c&        init_ue_id_c         = rrc_setup_req.rrc_setup_request.ue_id;
      asn1::fixed_bitstring<39>              con_res_id_bitstring = {};
      switch (init_ue_id_c.type().value) {
        case asn1::rrc_nr_r17::init_ue_id_c::types_opts::random_value:
          con_res_id_bitstring = init_ue_id_c.random_value();
          break;
        case asn1::rrc_nr_r17::init_ue_id_c::types_opts::ng_5_g_s_tmsi_part1:
          con_res_id_bitstring = init_ue_id_c.ng_5_g_s_tmsi_part1();
          break;
        default:
          break;
      }

      /* Append 0b10 to front */
      uint64_t con_res_id_num = 0b10;
      for (uint32_t i = con_res_id_bitstring.length(); i > 0; i--) {
        con_res_id_num |= con_res_id_bitstring.get(i - 1);
        con_res_id_num <<= 1;
      }
      /* Append 0b10000 to end */
      con_res_id_num <<= 4;
      con_res_id_num |= 0b10000;

      /* Convert to bytes */
      for (uint32_t i = srsran::mac_sch_subpdu_nr::ue_con_res_id_len; i > 0; i--) {
        uint8_t position_byte = 0xff & con_res_id_num;
        con_res_id_num >>= 8;
        con_res_id[i - 1] = position_byte;
      }
      return true;
    }
  }
  return false;
}

/* Since the RRC setup contains the contention resolution identity in SRSRAN,we have to replace the contentional
 * resolution id to the one used by UE in RRC setup request */
bool replace_con_res_id(srsran::mac_sch_pdu_nr                      original_rrc_setup,
                        const uint32_t                              origin_len,
                        srsran::mac_sch_subpdu_nr::ue_con_res_id_t& con_res_id,
                        srsran::byte_buffer_t&                      tx_buffer,
                        srslog::basic_logger&                       logger,
                        std::vector<uint8_t>*                       modified_dl_ccch_msg)
{
  srsran::mac_sch_pdu_nr rrc_setup_mac_pdu;
  rrc_setup_mac_pdu.init_tx(&tx_buffer, origin_len);
  /* Enumerate all subpdus */
  for (uint32_t i = 0; i < original_rrc_setup.get_num_subpdus(); i++) {
    srsran::mac_sch_subpdu_nr& subpdu = original_rrc_setup.get_subpdu(i);
    switch (subpdu.get_lcid()) {
      case srsran::mac_sch_subpdu_nr::nr_lcid_sch_t::CCCH: {
        /* If modified dl_ccch_msg is provided, then use modified dl_ccch_msg instead */
        if (modified_dl_ccch_msg) {
          rrc_setup_mac_pdu.add_sdu(subpdu.get_lcid(), modified_dl_ccch_msg->data(), modified_dl_ccch_msg->size());
        } else {
          rrc_setup_mac_pdu.add_sdu(subpdu.get_lcid(), subpdu.get_sdu(), subpdu.get_sdu_length());
        }
        break;
      }
      case srsran::mac_sch_subpdu_nr::nr_lcid_sch_t::CON_RES_ID: {
        /* Replace CON_RES_ID with provided con_res_id */
        rrc_setup_mac_pdu.add_ue_con_res_id_ce(con_res_id);
        break;
      }
      default: {
        rrc_setup_mac_pdu.add_sdu(subpdu.get_lcid(), subpdu.get_sdu(), subpdu.get_sdu_length());
        break;
      }
    }
  }

  /* Pack the updated RRC setup message */
  rrc_setup_mac_pdu.pack();
  return true;
}

bool modify_monitoring_symbol_within_slot(uint8_t*              dl_ccch_msg_raw,
                                          uint32_t              dl_ccch_msg_len,
                                          const std::string     symbol,
                                          std::vector<uint8_t>& modified_dl_ccch_msg)
{
  /* Parse to dl ccch msg */
  asn1::rrc_nr_r17::dl_ccch_msg_s dl_ccch_msg_orig;
  if (!parse_to_dl_ccch_msg(dl_ccch_msg_raw, dl_ccch_msg_len, dl_ccch_msg_orig)) {
    printf("Failed to parse DL-CCCH message\n");
    return false;
  }

  /* Check if it is RRC setup message */
  if (dl_ccch_msg_orig.msg.c1().type().value != asn1::rrc_nr_r17::dl_ccch_msg_type_c::c1_c_::types::rrc_setup) {
    printf("Not RRC setup message\n");
    return false;
  }

  /* Extract cell group cfg from dl_ccch_msg */
  asn1::rrc_nr_r17::cell_group_cfg_s cell_group_orig;
  if (!extract_cell_group_cfg(dl_ccch_msg_orig, cell_group_orig)) {
    printf("Failed to extract cell group config\n");
    return false;
  }

  /* Reach the specific element */
  if (!cell_group_orig.sp_cell_cfg_present) {
    return false;
  }
  if (!cell_group_orig.sp_cell_cfg.sp_cell_cfg_ded_present) {
    return false;
  }
  if (!cell_group_orig.sp_cell_cfg.sp_cell_cfg_ded.init_dl_bwp_present) {
    return false;
  }
  if (!cell_group_orig.sp_cell_cfg.sp_cell_cfg_ded.init_dl_bwp.pdcch_cfg_present) {
    return false;
  }

  /* Target element pdcch cfg */
  asn1::rrc_nr_r17::pdcch_cfg_s& pdcch_cfg = cell_group_orig.sp_cell_cfg.sp_cell_cfg_ded.init_dl_bwp.pdcch_cfg.setup();
  /* Modify the pdcch cfg */
  for (uint32_t i = 0; i < pdcch_cfg.search_spaces_to_add_mod_list.size(); i++) {
    asn1::rrc_nr_r17::search_space_s& search_space = pdcch_cfg.search_spaces_to_add_mod_list[i];
    search_space.monitoring_symbols_within_slot.from_string(symbol);
  }

  /* Pack the modified cell group cfg */
  asn1::dyn_octstring cell_group_modified;
  cell_group_modified.resize(512);
  asn1::bit_ref bref_cell_group(cell_group_modified.data(), cell_group_modified.size());
  if (cell_group_orig.pack(bref_cell_group) != asn1::SRSASN_SUCCESS) {
    printf("Failed to pack cell group config\n");
    return false;
  }
  cell_group_modified.resize(bref_cell_group.distance_bytes());

  /* Pack into dl ccch msg */
  asn1::rrc_nr_r17::rrc_setup_s   rrc_setup_orig = dl_ccch_msg_orig.msg.c1().rrc_setup();
  asn1::rrc_nr_r17::dl_ccch_msg_s ccch;
  asn1::rrc_nr_r17::rrc_setup_s&  rrc_setup_modified        = ccch.msg.set_c1().set_rrc_setup();
  rrc_setup_modified.rrc_transaction_id                     = rrc_setup_orig.rrc_transaction_id;
  asn1::rrc_nr_r17::rrc_setup_ies_s& rrc_setup_ies_modified = rrc_setup_modified.crit_exts.set_rrc_setup();
  rrc_setup_ies_modified.radio_bearer_cfg.srb_to_add_mod_list.resize(1);
  rrc_setup_ies_modified.master_cell_group = cell_group_modified;

  /* Pack the modified pdu */
  asn1::dyn_octstring dl_ccch_msg_modified;
  dl_ccch_msg_modified.resize(1024);
  asn1::bit_ref bref_dl_ccch_msg(dl_ccch_msg_modified.data(), dl_ccch_msg_modified.size());
  if (ccch.pack(bref_dl_ccch_msg) != asn1::SRSASN_SUCCESS) {
    printf("Failed to pack DL-CCCH message\n");
    return false;
  }
  dl_ccch_msg_modified.resize(bref_dl_ccch_msg.distance_bytes());
  modified_dl_ccch_msg.resize(dl_ccch_msg_modified.size());
  memcpy(modified_dl_ccch_msg.data(), dl_ccch_msg_modified.data(), dl_ccch_msg_modified.size());
  return true;
}