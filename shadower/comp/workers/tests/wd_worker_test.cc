#include "shadower/comp/workers/wd_worker.h"
#include "shadower/modules/dummy_exploit.h"
#include "shadower/utils/arg_parser.h"
#include "shadower/utils/safe_queue.h"
#include "shadower/utils/utils.h"

uint8_t                          buffer[2048];
uint32_t                         size;
direction_t                      direction       = DL;
bool                             packet_provided = false;
SafeQueue<std::vector<uint8_t> > dl_msg_queue;
SafeQueue<std::vector<uint8_t> > ul_msg_queue;

class TestExploit : public Exploit
{
public:
  TestExploit(SafeQueue<std::vector<uint8_t> >& dl_buffer_queue_, SafeQueue<std::vector<uint8_t> >& ul_buffer_queue_) :
    Exploit(dl_buffer_queue_, ul_buffer_queue_)
  {
    filters.clear();
    fields.clear();
  }
  ~TestExploit() = default;
  void setup() override
  {
    f_identity_response = ws_filter_t::make_filter("nas-5gs.mm.message_type == 0x5c");
    filters.push_back(f_identity_response);

    f_msin = ws_field_t::make_field_string("nas-5gs.mm.suci.msin");
    fields.push_back(f_msin);

    f_ack_sn = ws_field_t::make_field_uint32("rlc-nr.am.ack-sn");
    fields.push_back(f_ack_sn);
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
    if (f_identity_response->match) {
      logger.info("Identity Response filter matched");
    } else {
      logger.info("Identity Response filter did not match");
    }

    if (f_msin->has_string) {
      logger.info("Extracted MSIN: %s", f_msin->string_value);
    } else {
      logger.info("Failed to extract MSIN");
    }

    if (f_ack_sn->has_uint32) {
      logger.info("Extracted ACK-SN: %u", f_ack_sn->uint32_value);
    } else {
      logger.info("Failed to extract ACK-SN");
    }
  }

  std::shared_ptr<ws_filter_t> f_identity_response;
  std::shared_ptr<ws_field_t>  f_msin;
  std::shared_ptr<ws_field_t>  f_ack_sn;
};

void parse_args(int argc, char* argv[])
{
  int opt;
  while ((opt = getopt(argc, argv, "p:d:")) != -1) {
    switch (opt) {
      case 'p': {
        std::string packet = optarg;
        if (!hex_to_bytes(packet, buffer, &size)) {
          fprintf(stderr, "Failed to convert hex string to bytes.\n");
          exit(EXIT_FAILURE);
        }
        packet_provided = true;
        break;
      }
      case 'd': {
        std::string dir = optarg;
        if (dir == "dl") {
          direction = DL;
        } else if (dir == "ul") {
          direction = UL;
        } else {
          fprintf(stderr, "Unknown direction: %s\n", dir.c_str());
          exit(EXIT_FAILURE);
        }
        break;
      }
      default:
        fprintf(stderr, "Unknown option or missing argument.\n");
    }
  }
}

int main(int argc, char* argv[])
{
  parse_args(argc, argv);
  /* initialize logger */
  ShadowerConfig config;
  config.log_level             = srslog::basic_levels::debug;
  srslog::basic_logger& logger = srslog_init(&config);
  /* initialize wd worker */
  srsran_duplex_mode_t duplex_mode = srsran_duplex_mode_t::SRSRAN_DUPLEX_MODE_TDD;
  WDWorker             wd_worker(duplex_mode, config.log_level);
  TestExploit*         exploit = new TestExploit(dl_msg_queue, ul_msg_queue);
  exploit->setup();

  if (!packet_provided) {
    hex_to_bytes("01030001000124c00100013a0cbf00c249f003833f002e000680807888787f80000000304a3a80000000003d0039b8343f00",
                 buffer,
                 &size);
    direction = UL;
  }

  /* initialize exploit */
  wd_worker.process(buffer, size, 42000, 1, 1, 1, direction, exploit);
  return 0;
}