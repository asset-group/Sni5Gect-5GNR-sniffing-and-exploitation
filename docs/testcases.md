# Test files
The test files reside in the `shadower/test` folder are used for debugging purpose. There are some pre-defined testcases during development. 

The pre-defined testcases are defined as follows:
- test number 0: srsRAN band n78 with 20MHz bandwidth
- test number 1: srsRAN band n78 with 40MHz bandwidth
- test number 2: incomplete test files for Effnet not in use
- test number 4: srsRAN band n3 with 20MHz bandwidth
- test number 5: Singtel n1 with 20MHz bandwidth (incomplete)
- test number 6: srsRAN band n5 10MHz bandwidth

The raw IQ samples for these testcases are stored in `shadower/test/data/`.

The parameters for these testcases are defined in `shadower/test/test_variables.h`.

An example of the configuration `ShadowerConfig` defined for srsRAN n78 is defined as follows:
```
/* Shadower config for srsran n78 20MHz*/
ShadowerConfig srsran_n78_20MHz_config = {
    .band              = 78,  // Band number
    .nof_prb           = 51,  // Number of PRBs
    .scs_common        = srsran_subcarrier_spacing_30kHz, // Subcarrier spacing
    .scs_ssb           = srsran_subcarrier_spacing_30kHz, // Subcarrier spacing for SSB block
    .ssb_period_ms     = 10,  // SSB period in ms (in case if the period is too large and syncer cannot detect SSB block and turn to not in sync status)
    .ssb_period        = 20,  // SSB period in number of slots
    .dl_freq           = 3427.5e6, // downlink frequency 
    .ul_freq           = 3427.5e6, // uplink frequency 
    .ssb_freq          = 3421.92e6, // SSB frequency
    .sample_rate       = 23.04e6, // sample rate used
    .nof_channels      = 1,
    .uplink_cfo        = -0.00054, 
    .ssb_pattern       = SRSRAN_SSB_PATTERN_C, // SSB patterns need to be manually specified in the test files
    .duplex_mode       = SRSRAN_DUPLEX_MODE_TDD, // Duplex mode
    .delay_n_slots     = 5, 
    .duplications      = 4,
    .tx_cfo_correction = 0.0,
    .tx_advancement    = 160,
    .pdsch_mcs         = 3,
    .pdsch_prbs        = 24,
    .n_ue_dl_worker    = 4,
    .n_ue_ul_worker    = 4,
    .n_gnb_ul_worker   = 4,
    .n_gnb_dl_worker   = 4,
    .close_timeout     = 5000,
    .parse_messages    = true,
    .enable_gpu        = false,
    .enable_recorder   = false,
    .source_type       = "file",
    .source_params     = "/root/records/example.fc32",
    .source_module     = file_source_module_path,
    .log_level         = srslog::basic_levels::debug,
    .bc_worker_level   = srslog::basic_levels::debug,
    .worker_log_level  = srslog::basic_levels::debug,
    .syncer_log_level  = srslog::basic_levels::debug,
    .pcap_folder       = "/tmp/",
};
```

Moreover some of the other parameters need to be specified in the `init_test_args(int test_number)` function. https://github.com/asset-group/Sni5Gect-5GNR-sniffing-and-exploitation/blob/5f66d755bb1a45e1959960971fc17391d605bf50/shadower/test/test_variables.h#L306
```
test_args_t test_args = {};
  if (test_number == 0) {
    test_args.config            = srsran_n78_20MHz_config; // the ShadowerConfig defined above
    test_args.mib_config_raw    = "shadower/test/data/srsran-n78-20MHz/mib.raw"; // Where the decode MIB raw bytes will be stored
    test_args.sib_config_raw    = "shadower/test/data/srsran-n78-20MHz/sib1.raw"; // Where the decoded SIB raw bytes will be stored
    test_args.sib_size          = 101;  // SIB raw bytes size, this can be obtained after running the `sib_search_test` and look for SIB1 number of bytes
    test_args.rrc_setup_raw     = "shadower/test/data/srsran-n78-20MHz/rrc_setup.raw";  // Where the decoded RRC setup raw bytes will be stored
    test_args.rrc_setup_size    = 316; // rrc_setup raw bytes size
    test_args.rar_ul_grant_file = "shadower/test/data/srsran-n78-20MHz/rach_msg2_ul_grant.raw"; // The uplink transmission grant for msg3 is carried in the RAR message, after decoding RAR, the uplink grant is stored in this file
    test_args.c_rnti            = 17921; // Here assumes all the testcases for a test_number is for the same UE, so the RNTI should be same
    test_args.ra_rnti           = 267; // The RA-RNTI for detecting RAR
    test_args.ncellid           = 1; // The cell ID used after running the cell search
  }
```

Please note that after adding a completely new testcase with different configurations pre-defined, the following execution sequence is required to generate the required files:
1. `cell_search_test`: to generate the `mib_config_raw` file
2. `sib_search_test`: to generate the `sib_config_raw` file and `sib_size`
3. `rar_search_test`: to identify the correct TC-RNTI and generate the `rar_ul_grant_file` for `rach_msg3_test`
4. `rrc_setup_test`: to generate the `rrc_setup_raw` file and `rrc_setup_size`

## Cut the IQ sample files
To cut and align the IQ sample files into subframes, the easiest way is to set `enable_recorder` to true and run the `shadower` executable with offline recorded files, for example:
```
source:
  source_type: file
  source_params: /root/records/band3/round_1/output_ch_0.fc32,/root/records/band3/round_1/output_ch_1.fc32
```

## cell_search_test
This testcase is used to check if the cell search is working fine. You can identify the slot for example using the spectrogram:

Then the following information can be identified:
1. Slot index based on sfn from MIB
2. Decoded MIB information
3. Cell id
4. Offset to the start of the slot
5. CFO from calculation
```
[main   ] [I] SF index: 0 Slot index: 7680
[main   ] [I] Delay: 0.021092 us
[main   ] [I] Found cell: sfn=384 ssb_idx=0 hrf=n scs=30 ssb_offset=0 dmrs_typeA_pos=pos2 coreset0=0 ss0=0 barred=n intra_freq_reselection=n spare=0
[main   ] [I] Cell id: 1
[main   ] [I] Offset: 1650
[main   ] [I] CFO: -1.966215
```

## sib_search_test
For the SIB slot, there's two parameters that need to be provided. `slot_number` and `half`. 
For FDD band, since there's normally only one slot in the subframe, so half should be 0.
For subcarrier spacing > 15KHz, there's more than one slot in a subframe, so here need `half` parameter to indicate which slot in the subframe we should look the SIB into. But please note that in this case, the slot number in this case means the slot number for the start of the subframe, the actual slot number used to search for DCI is `slot_number + half`.
Example output:
1. DCI parameters for SIB transmission
2. PDSCH paraters from DCI and whether the decoding is successful
3. Decoded SIB1
4. SIB1 raw byte size
5. RA-RNTI from calculation
```
[main   ] [D] DCI DL slot 0 1: si-rnti=0xffff dci=1_0 ss=common0 L=2 cce=0 f_alloc=0xd8 t_alloc=0x0 vrb_to_prb_map=0 mcs=5 rv=0 sii=0 coreset0_bw=24 reserved=0x0 
[main   ] [D] PDSCH 0 1: si-rnti=0xffff prb=(0,9) symb=(2,13) CW0: mod=QPSK tbs=101 R=0.381 rv=0 CRC=OK iter=1.0 evm=0.00 epre=+8.3 snr=+22.4 cfo=+2.7 delay=+0.0 
[main   ] [I] SIB1: {}
[main   ] [I] SIB1 number of bytes: 101
[main   ] [I] RA-RNTI[0]: 253
[main   ] [I] RA-RNTI[1]: 267
```

## rar_search_test
In this testcase, other than the slot number and half. If the configuration changes, the RA-RNTI might also change. The potential RA-RNTI can be identified from the previous `sib_search_test`

Example output:
1. DCI parameters for RAR transmission
2. PDSCH paraters from DCI and whether the decoding is successful
3. TC-RNTI decoded from RAR
4. Timing advance command from the base station based on PRACH
5. The uplink transmission offset guess from the TAC
```
[main   ] [D] DCI DL slot 0 10: ra-rnti=0x010b dci=1_0 ss=common1 L=2 cce=0 f_alloc=0x30 t_alloc=0x0 vrb_to_prb_map=0 mcs=0 tb_scaling=0 reserved=0x0 
[main   ] [D] PDSCH 0 10: ra-rnti=0x10b prb=(0,2) symb=(2,13) CW0: mod=QPSK tbs=9 R=0.136 rv=0 CRC=OK iter=1.0 evm=0.00 epre=+7.6 snr=+28.8 cfo=+181.1 delay=+0.0 
[main   ] [I] TC-RNTI: 18168 RA-RNTI: 267
[main   ] [I] Time advance: 28
[main   ] [I] Uplink sample offset: 468
```
## rach_msg3_test
In this testcase, the following additional parameters is required to be provided.
1. `last_sample_file` if `half = 0`. as part of IQ samples for this uplink transmission reside in the last subframe
2. `rach_msg2_slot_idx` the slot number in which RAR is transmitted
3. `rach_msg3_slot_idx` the slot number in which the RACH message 3 is identified (start of the subframe)

Example output:
```
[main   ] [D] Setting RAR Grant: c-rnti=0x4601 dci=RAR ss=rar hop=0 f_alloc=0x69 t_alloc=0x0 mcs=0 tpc=7 csi=0 
[main   ] [D] PUSCH 0 17: c-rnti=0x4601 prb=(3,5) symb=(0,13) CW0: mod=QPSK tbs=11 R=0.131 rv=0 CRC=OK iter=1.0 evm=0.04 t_us=107 epre=+27.4 snr=+38.1 cfo=-2486.8 delay=+0.0 
```
## rrc_setup_test
The RRC Setup may be sent along with the contention resolution or after the contention resolution, at this time the TC-RNTI is set to the the C-RNTI with same value.
In this testcase, other than the slot number (start of the subframe) and the half, the rnti value have to be specified. This RNTI value can be identified from the RAR message.

It will show the LCID and the size of all the subpdus in the MAC, and only decode the CCCH message to RRC Setup and show it as json.

Example output:
```
[main   ] [D] DCI DL slot 0 10: c-rnti=0x46f9 dci=1_0 ss=common0 L=2 cce=0 f_alloc=0x5f t_alloc=0x0 vrb_to_prb_map=0 mcs=7 ndi=1 rv=0 harq_id=0 dai=0 pucch_tpc=1 pucch_res=3 harq_feedback=6 
[main   ] [D] PDSCH 0 10: c-rnti=0x46f9 prb=(0,21) symb=(2,13) CW0: mod=QPSK tbs=309 R=0.524 rv=0 CRC=OK iter=1.0 evm=0.00 epre=+8.8 snr=+19.7 cfo=+248.9 delay=+0.0 
[main   ] [I] LCID: 62 length: 6
[main   ] [I] LCID: 0 length: 299
[main   ] [D] CCCH message: {
  "c1": {
    "rrcSetup": {}}}
[main   ] [I] rrc_setup.raw sdu length: 299
[main   ] [I] 2040040932e00580088bd76380830f0003e0102341e0400020904c0ca8000ff800000000183708420001e01650020c00000081a6040514280038e2400040d55921070004103081430727122858c1a3879022000010a08010016a08020010a0a030016a0a040010a0c050016a0c061910a00071916a00081910a060918a09028628280b18a0b030628300d18a0d038628380f0120804004824110120a4092c6052623a2b3c4de4d03a41078bbf03043800000071ffa5294a529e502c0000432ec00000000000000000018ad5450047001800082000e21005c400e0202108001c4200b8401c080441000388401708038180842000710802e18070401104000e21005c300080000008218081018201c1a0001c71000000080100020180020240088029800008c40089c700180
```

## pdsch_decode_test
This testcase is to check whether the code can decode the PDSCH downlink message from the base station can be decoded correctly. The downlink message normally comes with PDCCH + PDSCH. So it runs DCI search with the provided configurations from SIB and RRC Setup message. Please ensure that the slot number (start of the subframe) + half is correctly identifying the slot. Moreover, please also ensure the RNTI (`args.c_rnti`) is been set to the correct value to ensure the DCI can be decoded successfully.

This testcase will search the DCI and apply the transmission grant from the DCI to decode the PDSCH. Then it will pass to the wdissector to generate the packet summary.

Example output:
```
[main   ] [D] DCI DL slot 0 0: c-rnti=0x4601 dci=1_1 ss=ue L=1 cce=4 f_alloc=0x14 t_alloc=0x0 mcs=27 ndi=0 rv=0 harq_id=0 dai=0 tpc=1 harq_feedback=3 ports=0 srs_request=0 dmrs_id=0 
[main   ] [D] PDSCH 0 0: c-rnti=0x4601 prb=(20,20) symb=(2,13) CW0: mod=64QAM tbs=84 R=0.910 rv=0 CRC=OK iter=1.0 evm=0.00 epre=+13.2 snr=+38.6 cfo=+1.8 delay=-0.0 
[main   ] [I] Decoded message: 01, 03, 00, 01, 00, 01, 0d, 90, 00, 00, 00, 28, 80, 8f, c0, 0b, 60, 20, 00, 00, 3f, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 
[wd_worker] [I] 17921 [S:0] --> [P:RLC-NR]  [DL] [AM] SRB:1  [CONTROL]  ACK_SN=1        ||    [DL] [AM] SRB:1  [DATA]     SN=0               [11-bytes..  (Padding 63 bytes) 
[main   ] [I] LCID: 1 length: 3
[main   ] [I] ACK sn
[main   ] [I] LCID: 1 length: 13
[main   ] [I] LCID: 63 length: 63
```

## pusch_decode_test
This testcase is much more complex. For an uplink transmission, the base station will normally send an DCI UL with uplink transmission grant to the UE. After a few slots, the UE will then apply such uplink transmission grant to send the message to the base station.

So in order to decode the uplink message, here, the slot carrying the DCI UL have to be specified. Please provide the `dci_sample_file` and `dci_slot_number` so that the testcase file will run the DCI search to get the uplink transmission grant. Also please ensure the RNTI is set to the correct value.

Then since it is an uplink transmission, so some of the IQ samples might be reside in the last slot. In order to provide the complete IQ samples, `last_sample_file` is required for testcases where `half = 0`. The parameter `ul_offset` specifies how many IQ samples are reside in the last slot, so that in the actual decoding, it will copy the tail `ul_offset` IQ samples to the buffer, along with the front part of the specified slot to decode the PUSCH tranmission.

Example output:
```
[main   ] [I] DCI sample file: shadower/test/data/srsran-n78-20MHz/dci_ul_3422.fc32
[main   ] [I] DCI slot number: 2
[main   ] [I] Sample file: shadower/test/data/srsran-n78-20MHz/pusch_3426.fc32
[main   ] [I] Slot number: 6
[main   ] [I] Half: 1
[main   ] [D] DCI UL slot 0 3: c-rnti=0x4601 dci=0_1 ss=ue L=1 cce=14 f_alloc=0x498 t_alloc=0x0 mcs=9 ndi=1 rv=0 harq_id=0 dai1=3 tpc=1 ports=2 srs_req=0 dmrs_id=0 ulsch=1 
[main   ] [D] PUSCH 0 7: c-rnti=0x4601 prb=(3,26) symb=(0,13) CW0: mod=QPSK tbs=528 R=0.670 rv=0 CRC=OK iter=1.0 evm=0.19 t_us=513 epre=+31.0 snr=+13.5 cfo=-2479.0 delay=-0.0 
[main   ] [I] PUSCH CRC passed
[wd_worker] [I] 17921 [S:0] <-- [P:NR RRC/NAS-5GS/NAS-5GS] RRC Setup Complete, Registration request, Registration request  [102-bytes]  (Short BSR LCG ID=0 BS=0)   (PHR PH=36 PCMAX_f_c=51)   (Padding 416 bytes) 
```