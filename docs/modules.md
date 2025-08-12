# Custom Exploit module

The exploit modules are designed to provide a flexible way to load different attack or exploits. When receiving a message, it will first send to wDissector to analyze the packet and if the packet matches with any Wireshark display filters specified, it will react according to the `post_dissection` specified, either inject messages to the communication or extract certain fields.

All exploit modules are extended from the `Exploit` class.

```cpp
#include "shadower/hdr/exploit.h"

class Exploit
{
public:
  Exploit(SafeQueue<std::vector<uint8_t> >& dl_buffer_queue_, SafeQueue<std::vector<uint8_t> >& ul_buffer_queue_) :
    dl_buffer_queue(dl_buffer_queue_), ul_buffer_queue(ul_buffer_queue_)
  {
  }

  virtual void setup() = 0;

  virtual void pre_dissection(wd_t* wd) = 0;

  virtual void post_dissection(wd_t*                 wd,
                               uint8_t*              buffer,
                               uint32_t              len,
                               uint8_t*              raw_buffer,
                               uint32_t              raw_buffer_len,
                               direction_t           direction,
                               uint32_t              slot_idx,
                               srslog::basic_logger& logger) = 0;

protected:
  SafeQueue<std::vector<uint8_t> >& dl_buffer_queue;
  SafeQueue<std::vector<uint8_t> >& ul_buffer_queue;
};


extern "C" {
__attribute__((visibility("default"))) Exploit* create_exploit(SafeQueue<std::vector<uint8_t> >& dl_buffer_queue_,
                                                               SafeQueue<std::vector<uint8_t> >& ul_buffer_queue_)
{
  return new DummyExploit(dl_buffer_queue_, ul_buffer_queue_);
}
}
```

The `setup()` function is used to initialize the filters, these filters are similar to the filters in wireshark, but have some simple difference, for instance `nas-5gs` has to change to `nas_5gs`. Following lines shows how to intialize these filters.

```cpp
f_rrc_setup_request      = wd_filter("nr-rrc.c1 == 0");
f_ack_sn                 = wd_field("rlc-nr.am.ack-sn");
```

The `pre_dissection` function is used to register your initialized fields before dissection. Simply use the following code to register the fields.

```cpp
wd_register_filter(wd, f_rrc_setup_request);
wd_register_field(wd, f_ack_sn);
```

The `post_dissection` function is the part where Sni5Gect decides whether to generate and send out the message to the UE. If the exploit script decides to inject a message to the UE, then it will push a new `std::shared_ptr<std::vector<uint8_t>>` buffer to the `dl_buffer_queue` queue, then later the UE DL injector will poll the queue, encode and then inject the signal to the target UE.
