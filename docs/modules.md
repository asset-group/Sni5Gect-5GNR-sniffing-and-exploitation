# Custom Exploit module

The exploit modules are designed to provide a flexible way to load different attacks or exploits. When receiving a message, it will first be sent to wDissector to analyze the packet and if the packet matches with any Wireshark display filters specified, it will react according to the `post_dissection` specified, either inject messages to the communication or extract certain fields.

All exploit modules are extensions from the `Exploit` class.

```cpp
#include "shadower/modules/exploit.h"

class DummyExploit : public Exploit
{
public:
  DummyExploit(SafeQueue<std::vector<uint8_t> >& dl_buffer_queue_, SafeQueue<std::vector<uint8_t> >& ul_buffer_queue_) :
    Exploit(dl_buffer_queue_, ul_buffer_queue_)
  {
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
  }
};

extern "C" {
__attribute__((visibility("default"))) Exploit* create_exploit(SafeQueue<std::vector<uint8_t> >& dl_buffer_queue_,
                                                               SafeQueue<std::vector<uint8_t> >& ul_buffer_queue_)
{
  return new DummyExploit(dl_buffer_queue_, ul_buffer_queue_);
}
}
```

The `setup()` function is responsible for initializing and registering Wireshark filters and fields. These filters now follow the same format as standard Wireshark display filters. The following lines demonstrate how to initialize and register these filters.

```cpp
f_rrc_setup_request = ws_filter_t::make_filter("nr-rrc.c1 == 0");
filters.push_back(f_rrc_setup_request);

f_ack_sn = ws_field_t::make_field_uint32("rlc-nr.am.ack-sn");
fields.push_back(f_ack_sn);
```

The `post_dissection` function is the part where Sni5Gect decides whether to generate and send out the message to the UE. If the exploit script decides to inject a message to the UE, then it will push a new `std::shared_ptr<std::vector<uint8_t>>` buffer to the `dl_buffer_queue` queue, then the UE DL injector will poll the queue, encode and then inject the signal to the target UE.
