#include "shadower/utils/constants.h"
#include "srsran/config.h"
#include <chrono>
#include <fstream>
#include <getopt.h>
#include <iostream>
#include <liquid/liquid.h>
#include <vector>

double      output_srate = 122.88e6;
double      input_srate  = 184.32e6;
std::string input_file;
std::string output_file;

void parse_args(int argc, char* argv[])
{
  int opt;
  while ((opt = getopt(argc, argv, "sSio")) != -1) {
    switch (opt) {
      case 's': {
        double outputSrateMHz = atof(argv[optind]);
        output_srate          = outputSrateMHz * 1e6;
        break;
      }
      case 'S': {
        double inputSrateMHz = atof(argv[optind]);
        input_srate          = inputSrateMHz * 1e6;
        break;
      }
      case 'i':
        input_file = argv[optind];
        break;
      case 'o':
        output_file = argv[optind];
        break;
      default:
        fprintf(stderr, "Unknown option: %c\n", opt);
        exit(EXIT_FAILURE);
    }
  }
}

int main(int argc, char* argv[])
{
  parse_args(argc, argv);
  std::ifstream in(input_file, std::ios::binary);
  if (!in.is_open()) {
    fprintf(stderr, "[ERROR] Failed to open input file: %s\n", input_file.c_str());
    exit(EXIT_FAILURE);
  }
  printf("Input file: %s opened successfully!\n", input_file.c_str());
  std::ofstream out(output_file, std::ios::binary);
  if (!out.is_open()) {
    fprintf(stderr, "[ERROR] Failed to open output file: %s\n", output_file.c_str());
    exit(EXIT_FAILURE);
  }
  printf("Output file: %s opened successfully!\n", output_file.c_str());

  // Create a liquid resampler object
  double                resample_rate = output_srate / input_srate;
  msresamp_crcf         resampler     = msresamp_crcf_create(resample_rate, TARGET_STOPBAND_SUPPRESSION);
  uint32_t              sf_len_in     = input_srate * SF_DURATION;
  uint32_t              sf_len_out    = output_srate * SF_DURATION;
  std::vector<cf_t>     input_buffer(sf_len_in);
  std::vector<cf_t>     output_buffer(sf_len_out);
  const std::streamsize expected_bytes = static_cast<std::streamsize>(sf_len_in * sizeof(cf_t));

  while (in.good()) {
    // Read the input buffer
    in.read(reinterpret_cast<char*>(input_buffer.data()), expected_bytes);
    if (in.gcount() != expected_bytes) {
      break; // End of file or read error
    }
    uint32_t num_output_samples;
    msresamp_crcf_execute(resampler,
                          (liquid_float_complex*)input_buffer.data(),
                          input_buffer.size(),
                          (liquid_float_complex*)output_buffer.data(),
                          &num_output_samples);
    // Write the output buffer to the file
    out.write(reinterpret_cast<char*>(output_buffer.data()), num_output_samples * sizeof(cf_t));
  }
  // Destroy the resampler object
  msresamp_crcf_destroy(resampler);
  return 0;
}