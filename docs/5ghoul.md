# Crash: 5Ghoul Attacks
These exploits are taken from paper [5Ghoul: Unleashing Chaos on 5G Edge Devices](https://asset-group.github.io/disclosures/5ghoul/). These affect the MTK modems of the OnePlus Nord CE2. 

|CVE|Module|
|---|------|
|CVE-2023-20702|lib_mac_sch_rrc_setup_crash_var.so|
|CVE-2023-32843|lib_mac_sch_mtk_rrc_setup_crash_3.so|
|CVE-2023-32842|lib_mac_sch_mtk_rrc_setup_crash_4.so|     
|CVE-2024-20003|lib_mac_sch_mtk_rrc_setup_crash_6.so|
|CVE-2023-32845|lib_mac_sch_mtk_rrc_setup_crash_7.so|

Upon receiving the `RRC Setup Request` message from the UE, Sni5Gect replies with malformed `RRC Setup` to the target UE. If the UE accepts such malformed `RRC Setup` message, it crashes immediately, this can be confirmed from the adb log containing keyword `sModemReason`, which indicates the MTK modem crashes. For example:

```
MDMKernelUeventObserver: sModemEvent: modem_failure
MDMKernelUeventObserver: sModemReason:fid:1567346682;cause:[ASSERT] file:mcu/l1/nl1/internal/md97/src/rfd/nr_rfd_configdatabase.c line:4380 p1:0x00000001
```
