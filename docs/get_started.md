# Get Started
## Hardware Requirements
Sni5Gect utilizes a USRP Software Defined Radio (SDR) device to send and receive IQ samples during communication between a legitimate 5G base station and a UE. Following SDRs are supported:
- USRP B210 SDR
- USRP x310 SDR

Host Machine Recommendations:
- Minimum: 12-core CPU
- 16 GB RAM

Our setup consists of AMD 5950x processor with 32 GB memory.

## Installation
Sni5Gect is modified from [srsRAN](https://github.com/srsran/srsRAN_4G) for message decoding and injection and it depends on [WDissector](https://github.com/asset-group/5ghoul-5g-nr-attacks) for dissecting the received messages.

The simplest way to setup the framework is using docker. We have provide the Dockerfile to build the whole framework from scratch. Please simply using the following command to build the container from scratch.
```bash
docker compose build
```

Then start the framework using:
```bash
docker compose up -d
```

## Run Sni5Gect
The Sni5Gect executable is located in the `build/shadower` directory, and configuration files are available in the `configs` folder.
Please use the following command to run Sni5Gect

```bash
docker exec -it sni5gect bash
./build/shadower/shadower configs/config-srsran-n78-20MHz.conf
```

### Use file recording
Running with Example Connection Recording
The easiest way to get started with Sni5Gect is to run it using a pre-recorded IQ sample file. We've provided a sample for offline testing.

Download and Extract the example recording file from Zenodo:
```bash
wget https://zenodo.org/records/15601773/files/example-connection-samsung-srsran.zip
unzip example-connection-samsung-srsran.zip
```

Edit configs/config-srsran-n78-20MHz.conf and modify the `[source]` section as follows:

```ini
[source]
source_type = file
source_module = build/shadower/libfile_source.so
# Replace with the absolute path to the extracted IQ sample file if needed
source_params = /root/sni5gect/example_connection/example.fc32  
```

Finally launch the sniffer using:

```bash
./build/shadower/shadower configs/config-srsran-n78-20MHz.conf
```

You should see output similar to the screenshot below:

<img src="https://raw.githubusercontent.com/asset-group/Sni5Gect-5GNR-sniffing-and-exploitation/main/images/example_recording.png"/>

### Use SDR

To test Sni5Gect with a live over-the-air signal using a Software Defined Radio (SDR), update the configuration file to use the SDR as the source.

Example `[source]` Section for UHD-compatible SDR (e.g., USRP B200)
```bash
[source]
source_type = uhd
source_module = build/shadower/libuhd_source.so
source_params = type=b200
```

Then start the sniffer with:
```bash
./build/shadower/shadower configs/config-srsran-n78-20MHz.conf
```
Upon startup, Sni5Gect will do the following:

1. Search for the base station using the specified center and SSB frequencies.
2. Retrieve cell configuration from SIB1.
3. Detect RAR messages indicating a new UE connecting to the target base station.

<img src="https://raw.githubusercontent.com/asset-group/Sni5Gect-5GNR-sniffing-and-exploitation/main/images/sni5gect-waiting-for-UE.png?token=GHSAT0AAAAAADF5L7NCLHQRUCRBMW5ZVXYQ2DXS7KQ"/>