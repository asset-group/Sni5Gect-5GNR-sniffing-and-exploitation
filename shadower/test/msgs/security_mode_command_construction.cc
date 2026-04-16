#include "shadower/comp/workers/wd_worker.h"
#include "shadower/modules/dummy_exploit.h"
#include "shadower/test/test_variables.h"
#include "shadower/utils/msg_helper.h"
#include "shadower/utils/safe_queue.h"
#include "shadower/utils/utils.h"

int main()
{
  ShadowerConfig config = {};
  config.log_level      = srslog::basic_levels::debug;

  /* initialize logger */
  srslog::basic_logger& logger = srslog_init(&config);
  logger.set_level(srslog::basic_levels::debug);
  /* Run wdissector for packet summary */
  WDWorker*                        wd_worker = new WDWorker(SRSRAN_DUPLEX_MODE_TDD, srslog::basic_levels::debug);
  SafeQueue<std::vector<uint8_t> > dl_msg_queue;
  SafeQueue<std::vector<uint8_t> > ul_msg_queue;
  DummyExploit*                    exploit = new DummyExploit(dl_msg_queue, ul_msg_queue);

  srsran::unique_byte_buffer_t           rrc_nr_buffer = srsran::make_byte_buffer();
  uint8_t                                rrc_nr_mac[4] = {0};
  asn1::rrc_nr_r17::dl_dcch_msg_s        dl_dcch_msg;
  asn1::rrc_nr_r17::security_mode_cmd_s& sec_mode_cmd = dl_dcch_msg.msg.set_c1().set_security_mode_cmd();
  sec_mode_cmd.rrc_transaction_id                     = 1;
  asn1::rrc_nr_r17::security_algorithm_cfg_s& algos =
      sec_mode_cmd.crit_exts.set_security_mode_cmd().security_cfg_smc.security_algorithm_cfg;
  algos.ciphering_algorithm              = asn1::rrc_nr_r17::ciphering_algorithm_opts::nea0;
  algos.integrity_prot_algorithm_present = true;
  algos.integrity_prot_algorithm         = asn1::rrc_nr_r17::integrity_prot_algorithm_opts::nia0;

  if (!pack_dl_dcch_to_rrc_nr(rrc_nr_buffer, dl_dcch_msg)) {
    logger.error("Failed to pack nas to rrc_nr\n");
    return -1;
  }

  /* Add AM header + PDCP header */
  srsran::unique_byte_buffer_t rlc_nr_buffer = srsran::make_byte_buffer();
  pack_rrc_nr_to_rlc_nr(rrc_nr_buffer->msg, rrc_nr_buffer->N_bytes, 0, 0, rrc_nr_mac, rlc_nr_buffer);

  /* Pack to mac-nr */
  srsran::byte_buffer_t mac_nr_buffer;
  pack_rlc_nr_to_mac_nr(rlc_nr_buffer->msg, rlc_nr_buffer->N_bytes, 0, mac_nr_buffer, 48);

  std::string mac_nr_hex = buffer_to_hex_string(mac_nr_buffer.msg, mac_nr_buffer.N_bytes);
  logger.info("MAC-NR: %s", mac_nr_hex.c_str());

  /* Dissector of rrc_nr encoded */
  wd_worker->process(mac_nr_buffer.msg, mac_nr_buffer.N_bytes, 0, 0, 0, 0, direction_t::DL, exploit);
}
