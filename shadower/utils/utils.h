#pragma once
#include "shadower/utils/arg_parser.h"
#include "srsran/asn1/rrc_nr.h"
#include "srsran/config.h"
extern "C" {
#include "srsran/phy/phch/prach.h"
}
#include "shadower/utils/constants.h"
#include "shadower/utils/dci_utils.h"
#include "shadower/utils/gnb_dl_utils.h"
#include "shadower/utils/gnb_ul_utils.h"
#include "shadower/utils/msg_helper.h"
#include "shadower/utils/phy_cfg_utils.h"
#include "shadower/utils/ssb_utils.h"
#include "shadower/utils/ue_dl_utils.h"
#include "shadower/utils/ue_ul_utils.h"
#include "srsran/srslog/srslog.h"
#include <inttypes.h>
#include <string>
#include <vector>

/* Write the IQ samples to a file so that we can use tools like matlab or spectrogram-py to debug */
void write_record_to_file(cf_t* buffer, uint32_t length, char* name = nullptr, const std::string& folder = "records");

/* Convert hex stream to uint8_t */
bool hex_to_bytes(const std::string& hex, uint8_t* buffer, uint32_t* size);

/* Turn a buffer to hex string */
std::string buffer_to_hex_string(uint8_t* buffer, uint32_t len);

/* Print and compare two different buffers */
bool compare_two_buffers(uint8_t* buffer1, uint32_t len1, uint8_t* buffer2, uint32_t len2);

/* Load the IQ samples from a file */
bool load_samples(const std::string& filename, cf_t* buffer, size_t nsamples);

/* Set the thread priority */
void set_thread_priority(std::thread& t, int priority);

/* Read binary form configuration dumped structure */
bool read_raw_config(const std::string& filename, uint8_t* buffer, size_t size);

/* Add the required UDP header for wdissector */
int add_fake_header(uint8_t*             buffer,
                    uint8_t*             data,
                    uint32_t             len,
                    uint16_t             rnti,
                    uint16_t             frame_number,
                    uint16_t             slot_number,
                    direction_t          direction,
                    srsran_duplex_mode_t duplex_mode);

/* Initialize logger */
srslog::basic_logger& srslog_init(ShadowerConfig* config);

/* Calculate the RA-rnti from SIB1 configuration */
std::vector<uint16_t> get_ra_rnti_list(asn1::rrc_nr::sib1_s& sib1, ShadowerConfig& config);