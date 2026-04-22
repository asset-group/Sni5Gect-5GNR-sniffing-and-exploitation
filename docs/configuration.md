# Configurations

An example configuration for srsRAN is provided in `configs/srsran-n78-20MHz-b210.yaml`.
For a new cell, the user has to first specify the band number. Sni5Gect needs the user to specify the center frequency and SSB frequency instead of searching these frequencies by itself. This information can be obtained using tools such as Cellular PRO. The frequency can be converted to arfcn using this website [5G Tools](https://5g-tools.com/5g-nr-arfcn-calculator/). Then, based on the bandwidth, the `nof_prb` has to be changed, this can normally be obtained from SIB1 either using qcSuper or Cellular Pro.

## Example Configuration
```yaml
cell:
  band: 78 # Band number
  nof_prb: 51 # Number of Physical Resource Blocks
  scs_common: 30 # Subcarrier Spacing for common (kHz)
  scs_ssb: 30 # Subcarrier Spacing for SSB (kHz)
  ssb_period_ms: 10 # SSB periodicity in milliseconds
  dl_arfcn: 628500 # Downlink ARFCN
  ssb_arfcn: 628128 # SSB ARFCN


source:
  source_type: uhd
  source_params: type=b200,master_clock_rate=23.04e6,serial=3218CC4

enable_recorder: false # record the IQ samples to file
pcap_folder: logs/

rf:
  sample_rate: 23.04e6
  num_channels: 1
  uplink_cfo: -0.00054 # Uplink Carrier Frequency Offset correction in Hz
  downlink_cfo: 0 # Downlink Carrier Frequency Offset (CFO) in Hz
  padding:
    front_padding: 0 # Number of IQ samples to pad in front of each burst
    back_padding: 0 # Number of IQ samples to pad at the end of each burst
  channels:
    - rx_frequency: 3427.5e6
      tx_frequency: 3427.5e6
      rx_offset: 0
      tx_offset: 0
      rx_gain: 40
      tx_gain: 80
      enable: true

workers:
  pool_size: 24 # Worker pool size
  n_ue_dl_worker: 4 # Number of UE downlink workers
  n_ue_ul_worker: 4 # Number of UE uplink workers
  n_gnb_dl_worker: 4 # Number of gNB downlink workers
  n_gnb_ul_worker: 4 # Number of gNB uplink workers

uetracker:
  close_timeout: 5000 # ms: stop tracking if no messages received
  parse_messages: true # Parse messages
  num_ues: 5 # Number of UETrackers to pre-initialize
  enable_gpu: false # GPU acceleration for decoding

downlink_injector:
  delay_n_slots: 5 # Number of slots to delay injecting the message
  duplications: 2 # Number of duplications to send in each inject
  tx_cfo_correction: 0 # Uplink CFO correction (Hz)
  tx_advancement: 160 # Number of samples to send in advance
  pdsch_mcs: 3 # PDSCH MCS
  pdsch_prbs: 24 # PDSCH PRBs


log:
  log_level: INFO
  syncer: INFO
  worker: INFO
  bc_worker: INFO

exploit: build/modules/lib_dummy_exploit.so # exploit module to use
# exploit:  build/modules/lib_identity_request.so
# exploit:  build/modules/lib_dg_authentication_request_sniffer.so
# exploit:  build/modules/lib_dg_authentication_replay.so
# exploit:  build/modules/lib_dg_registration_reject.so
# exploit:  build/modules/lib_mac_sch_mtk_rlc_crash.so
# exploit:  build/modules/lib_mac_sch_mtk_rrc_setup_crash_3.so
# exploit:  build/modules/lib_mac_sch_mtk_rrc_setup_crash_4.so
# exploit:  build/modules/lib_mac_sch_mtk_rrc_setup_crash_6.so
# exploit:  build/modules/lib_mac_sch_mtk_rrc_setup_crash_7.so
# exploit:  build/modules/lib_plaintext_registration_accept.so
```

## Special configurations
The following two configurations may change due to hardware differences. You may follow the instructions below to find the correct values.

The parameter `uplink_cfo_correction` is currently determined via a brute-force search using the code in `shadower/test/pusch_cfo_test.cc` The goal is to identify the value that allows decoding the highest number of uplink messages. 

From observations, this parameter is particularly important for srsRAN base stations operating in TDD bands. If uplink messages cannot be decoded in other bands, you may try setting this parameter to 0.

### tx_advancement
The `tx_advancement` parameter can also be determined through brute-force search. This can be done using `shadower/test/pdsch_decode_search.cc`, which attempts to decode injected message recordings with different offsets.

Alternatively, `tx_advancement` can be derived using the hardware test scripts:
- `shadower/test/hardware/sdr_timing_test.cc`
- `shadower/test/hardware/sdr_txrx_decode_test.cc`

These scripts are used to measure the delay between receiving a downlink SSB and transmitting a signal that aligns correctly at the receiver (i.e., RX-to-TX delay).
> Note: These tests require a base station running alongside the test scripts.
#### sdr_timing_test
This test performs the following steps:

1. Synchronizes with the base station using the syncer.
2. Transmits a generated SSB at a different frequency and slot.
3. A sender thread continuously transmit SSB block at a different frequency with the start time same as the base station slot boundary.
4. A receiver thread (or a second SDR) continuously receives the transmitted SSB and measures its timing offset relative to the base station.

Please refer to the example commands below, the first number is refering to the pre-defined parameters in `shadower/test/test_variables.h`. Please refer to `parse_args` in `shadower/test/hardware/sdr_timing_test.cc` for detailed parameters. `-R` will start the receiver thread and `-Z` will start the sender thread.
```
./build/shadower/test/sdr_timing_test 0 -s 23.04 -f 3427.5 -F 3427.5 -H 3427.5 -g 40 -G 80 -t 3421.92 -T 3424.8 -B 78 -d type=b200,master_clock_rate=23.04e6,clock=external -c 1 -r 100 -S 30 -P 20 -R -Z
```

Example output:
```
SSB Delay: 1829 sfn=100 ssb_idx=0 hrf=n scs=30 ssb_offset=14 dmrs_typeA_pos=pos2 coreset0=2 ss0=2 barred=n intra_freq_reselection=n spare=0 
2026-04-22T12:03:35.937933 [main   ] [I] Delay: 179 Count: 198
2026-04-22T12:03:35.937934 [main   ] [I] Delay: 2791 Count: 2
2026-04-22T12:03:35.937934 [main   ] [I] Min: 179 Max: 2791 Avg: 205
2026-04-22T12:03:35.937935 [main   ] [I] Avg CFO: 19.726157
```
In this example:
- The detected SSB offset is 1829 samples.
- The expected SSB position is 1650 samples (from configuration: 60 + 768 + 54 + 768).
- The difference (179 samples) represents the SDR hardware delay.

#### sdr_txrx_decode_test
After obtaining the delay, validate it by transmitting and decoding PDSCH using:

```
./build/shadower/test/sdr_txrx_decode_test -s 23.04 -d type=b200,master_clock_rate=23.04e6 -P 20 -S 30 -r 100 -t 179 -c 1 -F 1656 -b 1656
```

Expected output for a correct offset:
```
2026-04-22T12:11:38.537174 [main   ] [I] TX advancement: 179 Total sent: 100, DCI decoded: 98 (98.00%) PDSCH decoded: 98 (98.00%) PDSCH/DCI 100.00%
2026-04-22T12:11:39.537161 [main   ] [I] TX advancement: 179 Total sent: 200, DCI decoded: 198 (99.00%) PDSCH decoded: 198 (99.00%) PDSCH/DCI 100.00%
2026-04-22T12:11:40.537147 [main   ] [I] TX advancement: 179 Total sent: 300, DCI decoded: 298 (99.33%) PDSCH decoded: 298 (99.33%) PDSCH/DCI 100.00%
2026-04-22T12:11:41.537141 [main   ] [I] TX advancement: 179 Total sent: 400, DCI decoded: 398 (99.50%) PDSCH decoded: 398 (99.50%) PDSCH/DCI 100.00%
```

A high decoding success rate indicates that the selected tx_advancement is accurate.

> In practice, minor adjustments may still be required when performing injection tests with a COTS UE.

### ul_advancement

The `ul_advancement` parameter aligns the transmitted uplink signal with the base station’s expected timing. It can typically be estimated as:
- UE Timing Advance (TA) observed during sniffing
- + SDR transmit delay (measured via sdr_timing_test)

For example:
From RAR, we can get a collocated UE received TA=6 from RAR. The TA offset is 372 ($(TA * 16 * 64 / 2^miu + t_offset) * Tc * srate = (6 * 16 * 64 / 1 + 25600) * 1.0 / (480000.0 * 4096.0) * 23.04e6 = 372$) 

Then after running the `sdr_timing_test`, we can get the measured SDR delay for srsRAN n5 10MHz:
```
./build/shadower/test/sdr_timing_test 6 -s 11.52 -f 889 -F 889 -H 889 -g 40 -G 80 -t 887.05 -T 890.65 -B 5 -d type=b200,master_clock_rate=23.04e6,clock=external -c 1 -r 100 -S 15 -P 20 -R -Z
2026-04-22T12:23:12.674548 [main   ] [D] SSB Delay: 1776 sfn=100 ssb_idx=0 hrf=n scs=15 ssb_offset=14 dmrs_typeA_pos=pos2 coreset0=2 ss0=2 barred=n intra_freq_reselection=n spare=0 
2026-04-22T12:23:12.674550 [main   ] [I] Delay: 126 Count: 100
2026-04-22T12:23:12.674551 [main   ] [I] Min: 126 Max: 126 Avg: 126
2026-04-22T12:23:12.674552 [main   ] [I] Avg CFO: 1349.770597
```
So the measured SDR delay is 126. We can then derive the final `ul_advancement` as $126 + 372=498$.

This combined value should provide proper uplink timing alignment at the base station.