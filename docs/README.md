# Sni5Gect
> A framework for 5G NR sniffing and exploitation

Sni5Gect (Sniffing 5G Inject) is a framework designed to sniff unencrypted messages sent between the base station and the UE, and inject messages to target User Equipment (UE) over-the-air at specific states of 5G NR communication. It can be used to carry out attacks such as crashing the UE modem, downgrading to earlier generations of networks, fingerprinting, or authentication bypass.

<img src="https://raw.githubusercontent.com/asset-group/Sni5Gect-5GNR-sniffing-and-exploitation/main/images/sni5gect-overview.png">

## Updates

### v3.0 - Uplink Injection Support <span aria-hidden="true" style="color:#CB0000;">(5th November 2025)</span>
This release extends Sni5Gect with uplink injection capabilities:
- Uplink message injection: Leverage the uplink grant issued by the base station to craft and transmit forged uplink messages that appear to originate from the UE.
- Supported parameters
  - Subcarrier spacing: 15 kHz
  - Tested on: FDD band n5
- Hardware requirement: same as v1.0. Tested and validated using a B210 SDR.

**Notes:**
  - This feature enables spoofing of UE-originated uplink traffic by using observed grants; use responsibly and only in approved testbeds.
  - Not supported for subcarrier spacings of 30 kHz and above due to the hardware's stringent timing requirements.

### v2.0 – FDD Support <span aria-hidden="true" style="color:#CB0000;">(17th September 2025)</span>
This release expands Sni5Gect with new capabilities:
- Dual-channel FDD support: Receive and process IQ samples from two channels simultaneously using SDRs with dual RX chains.
- Unified codebase for 15 kHz and 30 kHz subcarrier spacing without recompilation.
- Expanded testing with the following setups:
    - Frequency Bands: 
        - TDD: n78, n41
        - FDD: n3
    - Frequency: 3427.5 MHz, 2550.15 MHz, 1865.0 MHz
    - Subcarrier Spacing: 30 kHz, 15 KHz

**Note:**
Since FDD uses separate frequencies for downlink and uplink, an SDR with two RF front-ends (e.g., USRP X310) is required to capture IQ samples from both frequencies simultaneously (otherwise Sni5Gect is only capable of sniffing the downlink channel). A GPSDO is strongly recommended for frequency synchronization; without it, manual calibration may be necessary.

### v1.0 – Initial Release <span aria-hidden="true" style="color:#CB0000;">(26th August 2025)</span>
The first release of Sni5Gect, supports TDD bands with a 30 kHz subcarrier spacing.
Tested configurations:
- Frequency Bands: n78, n41 (TDD)
- Frequency: 3427.5 MHz, 2550.15 MHz
- Subcarrier Spacing: 30 kHz
- Bandwidth: 20–50 MHz
- MIMO Configuration: Single-input single-output (SISO)
- Distance: 0 meter to up to 20 meters (with amplifier)

## Demo
### Demo: 5Ghoul Attack using Sni5Gect to crash UE modem
<iframe width="560" height="315" src="https://www.youtube.com/embed/CE006JIHzDM?si=wm-kJye5DzXSURsw" title="YouTube video player" frameborder="0" allow="accelerometer; autoplay; clipboard-write; encrypted-media; gyroscope; picture-in-picture; web-share" referrerpolicy="strict-origin-when-cross-origin" allowfullscreen></iframe>

### Demo: Multi-stage downgrade attack
<iframe width="560" height="315" src="https://www.youtube.com/embed/uDnHd02NT38?si=hcr8ygny6HBHSR01" title="YouTube video player" frameborder="0" allow="accelerometer; autoplay; clipboard-write; encrypted-media; gyroscope; picture-in-picture; web-share" referrerpolicy="strict-origin-when-cross-origin" allowfullscreen></iframe>

## Capability
- Downlink Control Information sniffing
- Downlink message sniffing (Sent from the base station to the UE)
- Uplink message sniffing (Sent from the UE to the base station)
- Downlink message injection (Pretend to be the base station and inject the message to the UE)

## Supported Features
We have tested with the following configurations:
- Frequency Bands: n78, n41 (TDD), n3 (FDD)
- Frequency: 3427.5 MHz, 2550.15 MHz, 1865.0 MHz
- Subcarrier Spacing: 30 kHz, 15 KHz
- Bandwidth: 20–50 MHz
- MIMO Configuration: Single-input single-output (SISO)
- Distance: 0 meter to up to 20 meters (with amplifier)

## Evaluated 5G base stations
Sni5Gect has been evaluated using srsRAN and Effnet as legitimate 5G base stations.
- [srsRAN 24.10.1](https://github.com/srsran/srsRAN_Project/releases/tag/release_24_10_1)
- [Effnet](https://www.effnet.com/) 2024-07-08 + [Phluido](https://www.phluido.net/) rru-0.8.7.4-uhd-3.15.0

## Overview of Components
Sni5Gect comprises several components, each responsible for handling different signals:
- Syncer: Synchronizes time and frequency with the target base station.
- Broadcast Worker: Decodes broadcast information such as SIB1 and detects and decodes RAR.
- UETracker: Tracks the connection between the UE and the base station.
- UE DL Worker: Decodes messages sent from the base station to the UE.
- GNB UL Worker: Decodes messages sent from the UE to the base station.
- GNB DL Injector: Encodes and injects messages to the UE.

![Signal and components](https://raw.githubusercontent.com/asset-group/Sni5Gect-5GNR-sniffing-and-exploitation/main/images/signal_components_match.svg)

## Project Structure
The project is organized as follows. The core Sni5Gect framework resides in the `shadower` directory. Key components are implemented in the following files:
```
.
├── cmake
├── configs
├── credentials
├── debian
├── images
├── lib
|-- shadower
|   |-- CMakeLists.txt
|   |-- comp
|   |   |-- CMakeLists.txt
|   |   |-- fft
|   |   |-- scheduler.cc  # Distributes received subframes to components
|   |   |-- ssb
|   |   |-- sync          # Syncer implementation
|   |   |-- trace_samples
|   |   |-- ue_tracker.cc # UE Tracker implementation
|   |   |-- ue_tracker.h
|   |   `-- workers
|   |       |-- CMakeLists.txt
|   |       |-- broadcast_worker.cc # Broadcast Worker implementation
|   |       |-- gnb_dl_worker.cc    # GNB DL Injector implementation
|   |       |-- gnb_ul_worker.cc    # GNB UL Worker implementation
|   |       |-- ue_dl_worker.cc     # UE DL Worker implementation
|   |       |-- wd_worker.cc        # wDissector wrapper
|   |-- main.cc
|   |-- modules   # Exploit modules
|   |-- source
|   |-- test
|   |-- tools
|   `-- utils
├── srsenb
├── srsepc
├── srsgnb
├── srsue
├── test
└── utils
```
