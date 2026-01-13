#include "shadower/source/source.h"
#include "shadower/utils/arg_parser.h"
#include "shadower/utils/buffer_pool.h"
#include "shadower/utils/constants.h"
#include "srsran/phy/utils/vector.h"
#include <atomic>
#include <complex>
#include <condition_variable>
#include <csignal>
#include <getopt.h>
#include <liquid/liquid.h>
#include <queue>

std::atomic<bool> stop_flag(false);

struct frame_t {
  uint32_t                            frames_idx;
  std::shared_ptr<std::vector<cf_t> > buffer[SRSRAN_MAX_CHANNELS];
  size_t                              buffer_size;
};

SharedBufferPool*                     buffer_pool = nullptr;
std::queue<std::shared_ptr<frame_t> > queue;
std::condition_variable               cv;
std::mutex                            mtx;

double         dl_freq       = 3427.5e6;
double         sample_rate   = 23.04e6;
double         gain          = 40;
uint32_t       num_frames    = 20000;
std::string    output_file   = "output";
std::string    output_folder = "/root/records/";
std::string    source_type   = "uhd";
std::string    device_args   = "type=b200";
ShadowerConfig config        = {};
uint32_t       sf_len        = 0;

void sigint_handler(int signum)
{
  if (stop_flag.load()) {
    if (signum == SIGINT) {
      printf("Received SIGINT, stopping...\n");
    } else if (signum == SIGTERM) {
      printf("Received SIGTERM, stopping...\n");
    }
  } else {
    exit(EXIT_FAILURE);
  }
  stop_flag.store(true);
  printf("Received signal %d, stopping...\n", signum);
}

void usage(const char* prog)
{
  printf("Usage: %s [options]\n", prog);
  printf("  -f <freq[,freq]>  DL,UL freq for channel 0 in MHz (if one value given, both RX/TX use it)\n");
  printf("  -F <freq[,freq]>  DL,UL freq for channel 1 in MHz\n");
  printf("  -g rx             RX,TX gains for channel 0\n");
  printf("  -G rx             RX,TX gains for channel 1\n");
  printf("  -s <MHz>          Sample rate in MHz\n");
  printf("  -t <str>          Source type\n");
  printf("  -d <str>          Source parameters\n");
  printf("  -n <n>            Number of test rounds\n");
  printf("  -o <file>         Output file name\n");
  printf("  -O <folder>       Output folder name\n");
}

static void parse_freq(const char* arg, double& rx, double& tx)
{
  double f1 = 0, f2 = 0;
  int    n = sscanf(arg, "%lf,%lf", &f1, &f2);
  if (n == 1) {
    rx = tx = f1 * 1e6;
  } else if (n == 2) {
    rx = f1 * 1e6;
    tx = f2 * 1e6;
  } else {
    fprintf(stderr, "Invalid frequency format: %s\n", arg);
    exit(EXIT_FAILURE);
  }
}

std::string join_path(const std::string& folder, const std::string& file)
{
  if (folder.empty()) {
    return file;
  }
  if (folder.back() == '/') {
    return folder + file;
  } else {
    return folder + "/" + file;
  }
}

void parse_args(int argc, char* argv[])
{
  int opt;
  config.channels.resize(2);
  memset(&config.channels[0], 0, sizeof(ChannelConfig));
  memset(&config.channels[1], 0, sizeof(ChannelConfig));
  while ((opt = getopt(argc, argv, "f:F:g:G:s:t:d:n:o:O:")) != -1) {
    switch (opt) {
      case 'f': {
        parse_freq(optarg, config.channels[0].rx_frequency, config.channels[0].tx_frequency);
        config.channels[0].enabled = true;
        break;
      }
      case 'F': {
        parse_freq(optarg, config.channels[1].rx_frequency, config.channels[1].tx_frequency);
        config.channels[1].enabled = true;
        break;
      }
      case 'g': {
        // Configure RX and TX gains for channel from command line
        config.channels[0].rx_gain = strtod(optarg, nullptr);
        config.channels[0].tx_gain = 0;
        break;
      }
      case 'G': {
        // Configure RX and TX gains for channel from command line
        config.channels[1].rx_gain = strtod(optarg, nullptr);
        config.channels[1].tx_gain = 0;
        break;
      }
      case 's':
        config.sample_rate = strtod(optarg, nullptr) * 1e6;
        break;
      case 't':
        config.source_type = optarg;
        break;
      case 'd':
        config.source_params = optarg;
        break;
      case 'n':
        num_frames = atoi(optarg);
        break;
      case 'o':
        output_file = optarg;
        break;
      case 'O':
        output_folder = optarg;
        break;
      default:
        usage(argv[0]);
        exit(EXIT_FAILURE);
    }
  }
  config.nof_channels = 0;
  for (uint32_t i = 0; i < 2; i++) {
    if (config.channels[i].enabled) {
      config.nof_channels++;
      printf("Channel %u enabled\n", i);
      printf("  RX frequency: %f MHz\n", config.channels[i].rx_frequency / 1e6);
      printf("  TX frequency: %f MHz\n", config.channels[i].tx_frequency / 1e6);
      printf("  RX gain: %f dB\n", config.channels[i].rx_gain);
      printf("  TX gain: %f dB\n", config.channels[i].tx_gain);
    }
  }

  printf("Sample Rate: %f MHz\n", config.sample_rate / 1e6);
  printf("Number of Channels: %d\n", config.nof_channels);
  printf("Device Args: %s\n", config.source_params.c_str());
  output_file = join_path(output_folder, output_file);
  sf_len      = config.sample_rate * SF_DURATION;
}

void receiver_worker()
{
  /* Initialize Source */
  Source* source = nullptr;
  if (source_type == "uhd") {
    create_source_t create_source = load_source(uhd_source_module_path);
    source                        = create_source(config);
  } else {
    fprintf(stderr, "Unknown source type: %s\n", source_type.c_str());
    exit(EXIT_FAILURE);
  }

  buffer_pool              = new SharedBufferPool(sf_len, 2048);
  srsran_timestamp_t ts    = {};
  uint32_t           count = 0;
  while (count++ < num_frames) {
    std::shared_ptr<frame_t> frame = std::make_shared<frame_t>();
    cf_t*                    rx_buffer[SRSRAN_MAX_CHANNELS];
    frame->frames_idx = count;

    for (uint32_t i = 0; i < config.nof_channels; i++) {
      std::shared_ptr<std::vector<cf_t> > buf = buffer_pool->get_buffer();
      frame->buffer[i]                        = buf;
      rx_buffer[i]                            = buf->data();
    }

    int result = source->recv(rx_buffer, sf_len, &ts);
    if (result == -1) {
      fprintf(stderr, "Failed to receive samples\n");
      break;
    }
    frame->buffer_size = result;
    {
      std::lock_guard<std::mutex> lock(mtx);
      queue.push(frame);
      cv.notify_one();
    }
  }
  stop_flag.store(true);
  source->close();
}

void writer_worker()
{
  // Create output files
  std::ofstream outfiles[SRSRAN_MAX_CHANNELS];
  if (config.nof_channels > 1) {
    for (uint32_t i = 0; i < config.nof_channels; i++) {
      outfiles[i].open(output_file + "_ch_" + std::to_string(i) + ".fc32", std::ios::binary);
      if (!outfiles[i].is_open()) {
        stop_flag.store(true);
        fprintf(stderr, "Failed to open output file\n");
        exit(EXIT_FAILURE);
      }
    }
  } else {
    outfiles[0].open(output_file + ".fc32", std::ios::binary);
    if (!outfiles[0].is_open()) {
      stop_flag.store(true);
      fprintf(stderr, "Failed to open output file\n");
      exit(EXIT_FAILURE);
    }
  }

  while (!stop_flag.load()) {
    std::shared_ptr<frame_t> frame = nullptr;
    {
      std::unique_lock<std::mutex> lock(mtx);
      cv.wait(lock, [] { return !queue.empty() || stop_flag.load(); });
      if (stop_flag.load()) {
        break;
      }
      frame = queue.front();
      queue.pop();
    }
    for (uint32_t i = 0; i < config.nof_channels; i++) {
      uint32_t num_output_samples = frame->buffer_size;
      outfiles[i].write((char*)frame->buffer[i]->data(), num_output_samples * sizeof(cf_t));
    }
    if (frame->frames_idx % 100 == 0) {
      printf(".");
      fflush(stdout);
    }
  }
}

int main(int argc, char* argv[])
{
  parse_args(argc, argv);
  std::signal(SIGINT, sigint_handler);
  pthread_t receiver_thread, writer_thread;
  // Create receiver thread
  if (pthread_create(&receiver_thread, nullptr, (void* (*)(void*))receiver_worker, nullptr) != 0) {
    fprintf(stderr, "Failed to create receiver thread\n");
    return -1;
  }
  // Set thread to the highest priority
  struct sched_param sp{};
  sp.sched_priority = sched_get_priority_max(SCHED_RR);
  if (pthread_setschedparam(receiver_thread, SCHED_RR, &sp) != 0) {
    fprintf(stderr, "Failed to set receiver thread priority\n");
    return -1;
  }
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(3, &cpuset);
  if (pthread_setaffinity_np(receiver_thread, sizeof(cpuset), &cpuset) != 0) {
    fprintf(stderr, "Failed to set receiver thread affinity\n");
    return -1;
  }

  int nproc = sysconf(_SC_NPROCESSORS_ONLN);
  for (int i = 0; i < nproc; i++) {
    printf("%d ", CPU_ISSET(i, &cpuset));
  }
  printf("\n");
  // Create writer thread
  if (pthread_create(&writer_thread, nullptr, (void* (*)(void*))writer_worker, nullptr) != 0) {
    fprintf(stderr, "Failed to create writer thread\n");
    return -1;
  }
  pthread_join(receiver_thread, nullptr);
  pthread_join(writer_thread, nullptr);
  return 0;
}