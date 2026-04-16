#include "shadower/comp/workers/wd_worker.h"
#include "shadower/modules/dummy_exploit.h"
#include "shadower/utils/msg_helper.h"
#include "shadower/utils/utils.h"
#include "srsran/mac/mac_sch_pdu_nr.h"
#include <chrono>
#include <iomanip>
#include <iostream>
#include <string>

int main()
{
  ShadowerConfig config = {};
  config.log_level      = srslog::basic_levels::debug;

  /* initialize logger */
  srslog::basic_logger& logger = srslog_init(&config);
  logger.set_level(srslog::basic_levels::debug);

  /* Initialize unique byte buffer*/
  srsran::unique_byte_buffer_t rrc_nr_buffer = srsran::make_byte_buffer();
  if (rrc_nr_buffer == nullptr) {
    logger.error("Failed to allocate buffer");
    return -1;
  }

  /* DL DCCH Message */
  asn1::rrc_nr_r17::dl_dcch_msg_s  dl_dcch_msg;
  asn1::rrc_nr_r17::rrc_release_s& release = dl_dcch_msg.msg.set_c1().set_rrc_release();
  release.rrc_transaction_id               = 0;

  /* RRC Release IEs */
  asn1::rrc_nr_r17::rrc_release_ies_s& ies = release.crit_exts.set_rrc_release();

  /* Set redirect to another carrier information */
  ies.redirected_carrier_info_present                      = true;
  asn1::rrc_nr_r17::carrier_info_nr_s& nr_redirection_info = ies.redirected_carrier_info.set_nr();

  asn1::rrc_nr_r17::redirected_carrier_info_eutra_s& redirection_info = ies.redirected_carrier_info.set_eutra();
  // redirection_info.cn_type         = asn1::rrc_nr_r17::redirected_carrier_info_eutra_s::cn_type_opts::epc;
  // redirection_info.cn_type_present = true;
  redirection_info.eutra_freq = 2680;

  asn1::json_writer writer;
  dl_dcch_msg.to_json(writer);
  logger.info("%s\n", writer.to_string().c_str());

  /* Encode the rrc release message into bytes */
  asn1::bit_ref bref{rrc_nr_buffer->data(), rrc_nr_buffer->get_tailroom()};
  if (dl_dcch_msg.pack(bref) != asn1::SRSASN_SUCCESS) {
    logger.error("Error packing rrc_release\n");
    return -1;
  }
  rrc_nr_buffer->resize(bref.distance_bytes());
  std::string rrc_nr_hex = buffer_to_hex_string(rrc_nr_buffer->msg, rrc_nr_buffer->N_bytes);
  logger.info("RRC-NR: %s", rrc_nr_hex.c_str());

  /* Add RRC header + PDCP header */
  srsran::unique_byte_buffer_t rlc_nr_buffer = srsran::make_byte_buffer();
  uint8_t                      rrc_nr_mac[4] = {0};
  pack_rrc_nr_to_rlc_nr(rrc_nr_buffer->msg, rrc_nr_buffer->N_bytes, 0, 0, rrc_nr_mac, rlc_nr_buffer);
  std::string rlc_nr_hex = buffer_to_hex_string(rlc_nr_buffer->msg, rlc_nr_buffer->N_bytes);
  logger.info("RLC-NR: %s", rlc_nr_hex.c_str());

  /* Add RLC header + MAC header */
  srsran::byte_buffer_t mac_nr_buffer;
  pack_rlc_nr_to_mac_nr(rlc_nr_buffer->msg, rlc_nr_buffer->N_bytes, 0, mac_nr_buffer, 32);
  std::string mac_nr_hex = buffer_to_hex_string(mac_nr_buffer.msg, mac_nr_buffer.N_bytes);
  logger.info("MAC-NR: %s", mac_nr_hex.c_str());

  /* Run wdissector for packet summary */
  WDWorker*                        wd_worker = new WDWorker(SRSRAN_DUPLEX_MODE_TDD, srslog::basic_levels::debug);
  SafeQueue<std::vector<uint8_t> > dl_msg_queue;
  SafeQueue<std::vector<uint8_t> > ul_msg_queue;
  DummyExploit*                    exploit = new DummyExploit(dl_msg_queue, ul_msg_queue);
  wd_worker->process(mac_nr_buffer.msg, mac_nr_buffer.N_bytes, 0x4601, 0, 0, 0, DL, exploit);
  return 0;
}