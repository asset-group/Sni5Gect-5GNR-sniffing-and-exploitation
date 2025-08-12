# Sni5Gect
> A framework for 5G NR sniffing and exploitation

Sni5Gect (Sniffing 5G Inject) is a framework designed to sniff unencrypted messages sent between the base station and the UE, and inject messages to target User Equipment (UE) over-the-air at specific states of 5G NR communication. This can be used to carry out attacks such as crashing the UE modem, downgrading to earlier generations of networks, fingerprinting, or authentication bypass.

<img src="https://raw.githubusercontent.com/asset-group/Sni5Gect-5GNR-sniffing-and-exploitation/main/images/sni5gect-overview.png">

## Capability
- Downlink Control Information sniffing
- Downlink message sniffing (Sent from the base station to the UE)
- Uplink message sniffing (Sent from the UE to the base station)
- Downlink message injection (Pretend to be the base station and inject the message to the UE)

## Supported Features
We have tested with the following configurations:
- Frequency Bands: n78, n41 (TDD)
- Frequency: 3427.5 MHz, 2550.15 MHz
- Subcarrier Spacing: 30 kHz
- Bandwidth: 20–50 MHz
- MIMO Configuration: Single-input single-output (SISO)
- Distance: 0 meter to upto 20 meters (with amplifier)

## Evaluated 5G base stations
Sni5Gect been evaluated using srsRAN and Effnet as legitimate 5G base stations.
- [srsRAN 24.10.1](https://github.com/srsran/srsRAN_Project/releases/tag/release_24_10_1)
- [Effnet](https://www.effnet.com/) 2024-07-08 + [Phluido](https://www.phluido.net/) rru-0.8.7.4-uhd-3.15.0

## Overview of Components
Sni5Gect comprises of several components, each responsible for handling different signals:
- Syncher: Synchronizes time and frequency with the target base station.
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
├── shadower
│   ├── hdr
│   ├── modules
│   ├── src
│   │   ├── broadcast_worker.cc # Broadcast Worker implementation
│   │   ├── gnb_dl_worker.cc    # GNB DL Injector implementation
│   │   ├── gnb_ul_worker.cc    # GNB UL Worker implementation
│   │   ├── scheduler.cc        # Distributes received subframes to components
│   │   ├── syncer.cc           # Syncher implementation
│   │   ├── ue_dl_worker.cc     # UE DL Worker implementation
│   │   ├── ue_tracker.cc       # UE Tracker implementation
│   │   └── wd_worker.cc        # wDissector wrapper
│   ├── test
│   └── tools
├── srsenb
├── srsepc
├── srsgnb
├── srsue
├── test
└── utils
```
