#include "shadower/comp/apis/apis.h"
#include "shadower/utils/utils.h"
#include <fstream>

int main()
{
  ShadowerConfig config        = {};
  config.log_level             = srslog::basic_levels::info;
  srslog::basic_logger& logger = srslog_init(&config);

  APIs apis;
  apis.init("ipc:///tmp/sni5gect-api.zmq");

  double sample_rate = 23.04e6;

  uplink_api_hdr_t msg = {};
  msg.rnti             = 0x4601;
  msg.rnti_type        = (uint16_t)srsran_rnti_type_t::srsran_rnti_type_c;
  msg.slot_idx         = 10;
  msg.task_idx         = 1;
  msg.sf_len           = sample_rate * SF_DURATION;
  msg.offset           = 360;
  msg.nof_prb          = 51;
  msg.start_symbol     = 1;
  msg.nof_symbol       = 13;
  for (uint32_t i = 0; i < 8; i++) {
    msg.prb_map[i] = true;
  }
  printf("size of time_t: %lu\n", sizeof(time_t));
  printf("size of double: %lu\n", sizeof(double));

  srsran_timestamp_t timestamp = {0, 0};
  srsran_timestamp_add(&timestamp, 1, 12345 / sample_rate);
  msg.full_secs = timestamp.full_secs;
  msg.frac_secs = timestamp.frac_secs;

  std::vector<cf_t> samples(msg.sf_len);
  if (!load_samples("shadower/test/data/srsran-n78-20MHz/pusch_3426.fc32", samples.data(), msg.sf_len)) {
    logger.error("Failed to load samples");
    return -1;
  }
  std::vector<cf_t> last_samples(msg.sf_len);
  if (!load_samples("shadower/test/data/srsran-n78-20MHz/pusch_3466.fc32", last_samples.data(), msg.sf_len)) {
    logger.error("Failed to load last samples");
    return -1;
  }

  for (uint32_t i = 0; i < 100; i++) {
    apis.send_uplink_api_message(msg, samples.data(), last_samples.data());
    msg.task_idx += 1;
    std::this_thread::sleep_for(std::chrono::seconds(1));
    printf("Sent uplink Api msg %u\n", msg.task_idx);
  }
  return 0;
}