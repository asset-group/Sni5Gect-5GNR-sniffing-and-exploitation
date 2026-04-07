#include "shadower/comp/workers/wd_worker.h"
#include "shadower/modules/dummy_exploit.h"
#include "shadower/test/test_variables.h"
#include "shadower/utils/msg_helper.h"
#include "shadower/utils/utils.h"
#include "srsran/asn1/asn1_utils.h"
#include "srsran/mac/mac_sch_pdu_nr.h"
#include <iomanip>
#include <sstream>

const uint8_t rrc_setup_request[] = {
    0x34, 0x12, 0xc8, 0x55, 0x51, 0x12, 0xd0, 0x39, 0x3e, 0x2f, 0x3e, 0x01,
    0x00, 0x3f, 0xf5, 0x06, 0xf3, 0x66, 0x2b, 0x66, 0x2d, 0x0a, 0xee,
};
const uint8_t original_rrc_setup[] = {
    0x3e, 0x17, 0x43, 0x8c, 0x73, 0xc7, 0x50, 0x00, 0x88, 0x28, 0x40, 0x04, 0x04, 0x1a, 0xe0, 0x05, 0x80, 0x08, 0x8b,
    0xd7, 0x63, 0x80, 0x83, 0x0f, 0x80, 0x03, 0xe0, 0x10, 0x23, 0x41, 0xe0, 0x40, 0x00, 0x20, 0x90, 0x4c, 0x0c, 0xa8,
    0x04, 0x0f, 0xff, 0xf8, 0x00, 0x00, 0x00, 0x08, 0x00, 0x01, 0xb8, 0xa2, 0x10, 0x00, 0x04, 0x00, 0xb2, 0x80, 0x00,
    0x24, 0x10, 0x00, 0x02, 0x20, 0x67, 0xa0, 0x6a, 0xa4, 0x9a, 0x80, 0x00, 0x20, 0x04, 0x04, 0x00, 0x08, 0x00, 0xd0,
    0x10, 0x01, 0x3b, 0x64, 0xb1, 0x80, 0xee, 0x03, 0xb3, 0xc4, 0xd5, 0xe6, 0x80, 0x00, 0x01, 0x4d, 0x08, 0x01, 0x00,
    0x01, 0x2c, 0x0e, 0x10, 0x41, 0x64, 0xe0, 0xc1, 0x0e, 0x00, 0x1c, 0x4a, 0x07, 0x00, 0x00, 0x08, 0x17, 0xbd, 0x00,
    0x40, 0x00, 0x40, 0x00, 0x01, 0x90, 0x00, 0x50, 0x00, 0xca, 0x81, 0x80, 0x62, 0x20, 0x0a, 0x80, 0x00, 0x00, 0x00,
    0x00, 0xa1, 0x00, 0x40, 0x00, 0x0a, 0x28, 0x40, 0x40, 0x01, 0x63, 0x00};

int main()
{
  ShadowerConfig config = {};
  config.log_level      = srslog::basic_levels::debug;

  srslog::basic_logger& logger = srslog_init(&config);
  logger.set_level(srslog::basic_levels::debug);

  /* Extract the contention resolution id first */
  srsran::mac_sch_subpdu_nr::ue_con_res_id_t con_res_id = {};
  if (!extract_con_res_id(rrc_setup_request, sizeof(rrc_setup_request), con_res_id, logger)) {
    logger.error("Failed to extract con_res_id");
    return -1;
  }

  std::ostringstream oss;
  for (uint32_t i = 0; i < srsran::mac_sch_subpdu_nr::ue_con_res_id_len; i++) {
    oss << std::setfill('0') << std::setw(2) << std::hex << static_cast<int>(con_res_id.data()[i]);
  }
  logger.info("Extracted identity: %s", oss.str().c_str());

  /* Unpack RRC setup */
  srsran::mac_sch_pdu_nr original_rrc_setup_mac_pdu;
  if (original_rrc_setup_mac_pdu.unpack(original_rrc_setup, sizeof(original_rrc_setup))) {
    logger.error("Failed to unpack original RRC setup message");
    return -1;
  }

  /* Enumerate all subpdus */
  std::vector<uint8_t> original_dl_ccch_msg;
  for (uint32_t i = 0; i < original_rrc_setup_mac_pdu.get_num_subpdus(); i++) {
    srsran::mac_sch_subpdu_nr& subpdu = original_rrc_setup_mac_pdu.get_subpdu(i);
    if (subpdu.get_lcid() == srsran::mac_sch_subpdu_nr::nr_lcid_sch_t::CCCH) {
      /* Parse to dl ccch msg */
      original_dl_ccch_msg.resize(subpdu.get_sdu_length());
      original_dl_ccch_msg.assign(subpdu.get_sdu(), subpdu.get_sdu() + subpdu.get_sdu_length());
    }
  }

  // /* Check if modify the monitoring symbol works */
  // std::vector<uint8_t> modified_dl_ccch_msg;
  // std::string          symbol = "10000010110110";
  // /* Check if modify the monitoring symbol works */
  // if (!modify_monitoring_symbol_within_slot(
  //         original_dl_ccch_msg.data(), original_dl_ccch_msg.size(), symbol, modified_dl_ccch_msg)) {
  //   printf("Failed to modify monitoring symbol\n");
  //   return -1;
  // }

  /* Replace contention resolution identity in rrc_setup */
  srsran::byte_buffer_t tx_buffer;
  if (!replace_con_res_id(original_rrc_setup_mac_pdu,
                          sizeof(original_rrc_setup),
                          con_res_id,
                          tx_buffer,
                          logger,
                          &original_dl_ccch_msg)) {
    // &modified_dl_ccch_msg)) {
    logger.info("Failed to replace con_res_id");
    return -1;
  }

  /* Run wdissector for packet summary */
  WDWorker*                        wd_worker = new WDWorker(config.duplex_mode, config.log_level);
  SafeQueue<std::vector<uint8_t> > dl_msg_queue;
  SafeQueue<std::vector<uint8_t> > ul_msg_queue;
  DummyExploit*                    exploit = new DummyExploit(dl_msg_queue, ul_msg_queue);
  wd_worker->process(tx_buffer.data(), tx_buffer.N_bytes, 1234, 0, 0, 0, DL, exploit);

  std::ostringstream oss_origin;
  std::ostringstream oss_received;
  std::ostringstream oss_encoded;

  for (uint32_t i = 0; i < sizeof(original_rrc_setup); i++) {
    oss_origin << "0x" << std::setfill('0') << std::setw(2) << std::hex << static_cast<int>(original_rrc_setup[i])
               << ", ";

    if (original_rrc_setup[i] != tx_buffer.data()[i]) {
      oss_encoded << RED "0x" << std::setfill('0') << std::setw(2) << std::hex << static_cast<int>(tx_buffer.data()[i])
                  << ", " RESET;
    } else {
      oss_encoded << "0x" << std::setfill('0') << std::setw(2) << std::hex << static_cast<int>(tx_buffer.data()[i])
                  << ", ";
    }
  }

  logger.info("Original RRC setup: %s", oss_origin.str().c_str());
  logger.info(" Encoded RRC setup: %s", oss_encoded.str().c_str());
  usleep(10000);
}