# Configurations

An example configuration for srsRAN is provided in `configs/config-srsran-n78-20MHz.conf`.
For a new cell, the user has to firstly specify the band number. Sni5Gect needs the user to specify the center frequency and SSB frequency instead of searching these frequencies by itself. These informations can be obtained using tools such as Cellular PRO. The frequency can be converted to arfcn using this website [5G Tools](https://5g-tools.com/5g-nr-arfcn-calculator/). Then, based on the bandwidth, the `nof_prb` have to be changed, this can normally be obtained from SIB1 either using `qcsuper` or Cellular Pro.

## Example Configuration
```ini
[cell]
band = 78       # 5G Band number used
nof_prb = 51    # Number of Physical Resource Blocks, obtained from srsRAN base station
scs_common = 30 # Subcarrier Spacing for common (kHz)
scs_ssb = 30    # Subcarrier Spacing for SSB (kHz)

[rf]
freq_offset = 0        # Frequency offset (Hz)
tx_gain = 80           # Transmit gain (dB)
rx_gain = 40           # Receive gain (dB)
dl_arfcn = 628500      # Downlink ARFCN
ssb_arfcn = 628128     # SSB ARFCN
sample_rate = 23.04e6  # Sample rate (Hz)
uplink_cfo_correction = -0.00054  # Uplink CFO (Hz) correction

[recorder]
enable_recorder = false # Enable recording the IQ samples to a file
recorder_file = /tmp/output.fc32 # Recording output file

[task]
slots_to_delay = 5         # Number of slots to delay injecting the message
max_flooding_epoch = 4     # Number of duplications to send in each inject
tx_cfo_correction = 0      # Uplink CFO correction (Hz)
send_advance_samples = 160 # Number of samples to send in advance
n_ue_dl_worker = 4         # Number of UE downlink workers
n_ue_ul_worker = 4         # Number of UE uplink workers
n_gnb_dl_worker = 4        # Number of gNB downlink workers
n_gnb_ul_worker = 4        # Number of gNB uplink workers
pdsch_mcs = 3              # PDSCH MCS for injection
pdsch_prbs = 24            # PDSCH PRBs for injection
close_timeout = 5000       # Close timeout, after how long haven't received a message should stop tracking the UE (ms)


[source]
source_type = uhd # Source type: file, uhd
source_module = build/shadower/libuhd_source.so
source_params = type=b200,serial=3218CC4  # Device arguments for SDR source

[log]
log_level = INFO           # General log level
syncer_log_level = INFO    # Syncer log level
worker_log_level = INFO    # Worker log level: Set to DEBUG to observe the DCI information
bc_worker_log_level = INFO # Broadcast worker log level

[pcap]
pcap_folder = logs/ # Pcap folder

[worker]
pool_size = 20 # Worker pool size
num_ues = 12   # Number of UETrackers to pre-initialize
enable_gpu_acceleration = false


[exploit]
module = modules/lib_dummy.so # Note only one exploit module can be loaded each time
```

## Special configurations
The following two configurations may change due to the hardware differences. You may follow the instructions below to find the correct values.

The parameter `uplink_cfo_correction` is found by bruteforce search using the code in `shadower\test\pusch_cfo_test.cc` to identify the best value that can decode the most number of uplink messages. 

The parameter `send_advance_samples` can also be found by bruteforce search currently, it can be done by using the code `shadower\test\pdsch_decode_search.cc` to try to decode the injected message recordings.
