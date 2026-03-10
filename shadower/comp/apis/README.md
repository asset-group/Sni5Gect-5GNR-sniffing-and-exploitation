# Python Uplink API

Stream decoded uplink (UE-to-gNB) data from the Sni5Gect sniffer to Python clients.

---

## Table of Contents

- [Overview](#overview)
- [Prerequisites](#prerequisites)
- [Enabling the API](#enabling-the-api)
- [Message Format](#message-format)
  - [Header Struct](#header-struct)
  - [ZMQ Multipart Frames](#zmq-multipart-frames)
- [Quick Start](#quick-start)
- [Additional Examples](#additional-examples)
  - [Logging SNR Over Time](#logging-snr-over-time)
  - [Plotting IQ Constellation](#plotting-iq-constellation)
  - [Filtering by RNTI](#filtering-by-rnti)
- [API Reference](#api-reference)
  - [`receive(socket)`](#receivesocket)
  - [`show(msg)`](#showmsg)
- [Testing](#testing)

---

## Overview

The Python Uplink API uses a **ZeroMQ PUB/SUB** pattern over IPC to stream decoded uplink data from the C++ Sni5Gect sniffer to Python clients. Whenever the sniffer decodes a PUSCH transmission from a UE, it publishes the decoded header metadata together with the raw IQ samples so that external Python scripts can perform further analysis, logging, or visualization.

- **Transport:** ZeroMQ IPC socket
- **Address:** `ipc:///tmp/sni5gect-ul-api.zmq`
- **Message structure:** Each message is a **3-frame ZMQ multipart message** containing a packed header struct, the previous subframe's IQ samples, and the current subframe's IQ samples.

The canonical Python receiver implementation is in [`receiver.py`](receiver.py).

---

## Prerequisites

- **Python 3**
- **pyzmq** and **numpy** packages:

```bash
pip install pyzmq numpy
```

---

## Enabling the API

Add the `expose_data` flag to your YAML configuration file at the root level (alongside `enable_recorder`, `pcap_folder`, etc.):

```yaml
enable_recorder: false
pcap_folder: "logs/"
expose_data: true          # <-- enables the Python Uplink API
```

When `expose_data` is `true`:

1. The YAML parser reads the flag (`shadower/utils/src/arg_parser.cc`, line 122).
2. The `Scheduler` constructor creates an `APIs` instance and binds the ZMQ PUB socket to `ipc:///tmp/sni5gect-ul-api.zmq` (`shadower/comp/scheduler.cc`, lines 32-39).
3. Each `UETracker` is given a reference to the API object so that uplink decode workers can publish messages.

---

## Message Format

### Header Struct

The C++ header is the packed struct `uplink_api_hdr_t` defined in [`apis.h`](apis.h) (lines 8-24). The Python side unpacks it with `struct.unpack` using little-endian byte order:

| Field | C Type | Python `struct` Format | Description |
|---|---|---|---|
| `message_type` | `uint32_t` | `I` | Always `1` (uplink message) |
| `rnti` | `uint16_t` | `H` | UE Radio Network Temporary Identifier |
| `rnti_type` | `uint16_t` | `H` | RNTI type (e.g., C-RNTI) |
| `slot_idx` | `uint32_t` | `I` | Slot index |
| `task_idx` | `uint32_t` | `I` | Internal task index |
| `sf_len` | `uint32_t` | `I` | Subframe length in samples |
| `offset` | `uint32_t` | `I` | Timing advance in samples |
| `snr_dB` | `float` | `f` | Measured SNR in dB |
| `full_secs` | `time_t` (int64) | `q` | Estimated TOA — integer seconds |
| `frac_secs` | `double` | `d` | Fractional seconds |
| `time_diff` | `double` | `d` | Propagation delay estimate (seconds) |
| `nof_prb` | `uint32_t` | `I` | Number of PRBs allocated |
| `start_symbol` | `uint32_t` | `I` | Start OFDM symbol index |
| `nof_symbol` | `uint32_t` | `I` | Number of OFDM symbols |
| `prb_map` | `bool[275]` | `275s` | Per-PRB allocation bitmap (one byte per PRB) |

The full format string used in Python:

```python
SRSRAN_MAX_PRB_NR = 275

Uplink_API_HDR_FMT = (
    "<"     # little-endian
    "I"     # message_type
    "H"     # rnti
    "H"     # rnti_type
    "I"     # slot_idx
    "I"     # task_idx
    "I"     # sf_len
    "I"     # offset
    "f"     # snr_dB
    "q"     # full_secs
    "d"     # frac_secs
    "d"     # time_diff
    "I"     # nof_prb
    "I"     # start_symbol
    "I"     # nof_symbol
    f"{SRSRAN_MAX_PRB_NR}s"  # prb_map
)

Uplink_API_HDR_SIZE = struct.calcsize(Uplink_API_HDR_FMT)
```

### ZMQ Multipart Frames

Each ZMQ message consists of three frames:

| Frame | Content | Format |
|---|---|---|
| **Frame 1** | Packed header (`uplink_api_hdr_t`) | Little-endian binary, `Uplink_API_HDR_SIZE` bytes |
| **Frame 2** | `last_ul_buffer` — previous subframe IQ samples | `complex64` array (interleaved `float32` real/imag pairs) |
| **Frame 3** | `ul_buffer` — current subframe IQ samples | `complex64` array (interleaved `float32` real/imag pairs) |

The number of IQ samples in each buffer is given by the `sf_len` field in the header. Use `numpy.frombuffer` with `dtype=np.complex64` and `count=sf_len` to decode:

```python
ul_iq = np.frombuffer(frame_3, dtype=np.complex64, count=sf_len)
```

---

## Quick Start

The following example connects to the Sni5Gect uplink API and prints a summary line for every decoded uplink message. This is the core loop from [`receiver.py`](receiver.py):

```python
import zmq
import struct
import numpy as np

ZMQ_ADDR = "ipc:///tmp/sni5gect-ul-api.zmq"
SRSRAN_MAX_PRB_NR = 275
CF_T_DTYPE = np.complex64

Uplink_API_HDR_FMT = (
    "<"
    "I"   # message_type
    "H"   # rnti
    "H"   # rnti_type
    "I"   # slot_idx
    "I"   # task_idx
    "I"   # sf_len
    "I"   # offset
    "f"   # snr_dB
    "q"   # full_secs
    "d"   # frac_secs
    "d"   # time_diff
    "I"   # nof_prb
    "I"   # start_symbol
    "I"   # nof_symbol
    f"{SRSRAN_MAX_PRB_NR}s"  # prb_map
)
Uplink_API_HDR_SIZE = struct.calcsize(Uplink_API_HDR_FMT)

ctx = zmq.Context()
sock = ctx.socket(zmq.SUB)
sock.connect(ZMQ_ADDR)
sock.setsockopt(zmq.SUBSCRIBE, b"")

while True:
    frames = sock.recv_multipart()
    hdr_buf, last_buf, buf = frames
    hdr = struct.unpack(Uplink_API_HDR_FMT, hdr_buf[:Uplink_API_HDR_SIZE])
    sf_len = hdr[5]
    ul_iq = np.frombuffer(buf, dtype=np.complex64, count=sf_len)
    last_ul_iq = np.frombuffer(last_buf, dtype=np.complex64, count=sf_len)
    print(f"RNTI: {hdr[1]:#06x}, Slot: {hdr[3]}, SNR: {hdr[7]:.1f} dB, Samples: {sf_len}")
```

---

## Additional Examples

### Logging SNR Over Time

Write per-message SNR readings to a CSV file with timestamps:

```python
import zmq, struct, numpy as np, csv, datetime

ZMQ_ADDR = "ipc:///tmp/sni5gect-ul-api.zmq"
SRSRAN_MAX_PRB_NR = 275

Uplink_API_HDR_FMT = (
    "<"
    "I"   # message_type
    "H"   # rnti
    "H"   # rnti_type
    "I"   # slot_idx
    "I"   # task_idx
    "I"   # sf_len
    "I"   # offset
    "f"   # snr_dB
    "q"   # full_secs
    "d"   # frac_secs
    "d"   # time_diff
    "I"   # nof_prb
    "I"   # start_symbol
    "I"   # nof_symbol
    f"{SRSRAN_MAX_PRB_NR}s"  # prb_map
)
Uplink_API_HDR_SIZE = struct.calcsize(Uplink_API_HDR_FMT)

ctx = zmq.Context()
sock = ctx.socket(zmq.SUB)
sock.connect(ZMQ_ADDR)
sock.setsockopt(zmq.SUBSCRIBE, b"")

with open("snr_log.csv", "w", newline="") as f:
    writer = csv.writer(f)
    writer.writerow(["timestamp", "rnti", "slot_idx", "snr_dB"])
    while True:
        frames = sock.recv_multipart()
        hdr = struct.unpack(Uplink_API_HDR_FMT, frames[0][:Uplink_API_HDR_SIZE])
        ts = datetime.datetime.fromtimestamp(hdr[8] + hdr[9])
        writer.writerow([ts.isoformat(), f"0x{hdr[1]:04x}", hdr[3], f"{hdr[7]:.2f}"])
```

### Plotting IQ Constellation

Capture a single uplink message and plot its IQ constellation diagram:

```python
import zmq, struct, numpy as np, matplotlib.pyplot as plt

ZMQ_ADDR = "ipc:///tmp/sni5gect-ul-api.zmq"
SRSRAN_MAX_PRB_NR = 275

Uplink_API_HDR_FMT = (
    "<"
    "I"   # message_type
    "H"   # rnti
    "H"   # rnti_type
    "I"   # slot_idx
    "I"   # task_idx
    "I"   # sf_len
    "I"   # offset
    "f"   # snr_dB
    "q"   # full_secs
    "d"   # frac_secs
    "d"   # time_diff
    "I"   # nof_prb
    "I"   # start_symbol
    "I"   # nof_symbol
    f"{SRSRAN_MAX_PRB_NR}s"  # prb_map
)
Uplink_API_HDR_SIZE = struct.calcsize(Uplink_API_HDR_FMT)

ctx = zmq.Context()
sock = ctx.socket(zmq.SUB)
sock.connect(ZMQ_ADDR)
sock.setsockopt(zmq.SUBSCRIBE, b"")

frames = sock.recv_multipart()
hdr = struct.unpack(Uplink_API_HDR_FMT, frames[0][:Uplink_API_HDR_SIZE])
sf_len = hdr[5]
ul_iq = np.frombuffer(frames[2], dtype=np.complex64, count=sf_len)

plt.scatter(ul_iq.real, ul_iq.imag, s=1, alpha=0.3)
plt.xlabel("I")
plt.ylabel("Q")
plt.title(f"UL IQ — RNTI 0x{hdr[1]:04x}, SNR {hdr[7]:.1f} dB")
plt.axis("equal")
plt.grid(True)
plt.show()
```

### Filtering by RNTI

Process only messages from a specific UE by checking the RNTI field:

```python
import zmq, struct, numpy as np

ZMQ_ADDR = "ipc:///tmp/sni5gect-ul-api.zmq"
SRSRAN_MAX_PRB_NR = 275

Uplink_API_HDR_FMT = (
    "<"
    "I"   # message_type
    "H"   # rnti
    "H"   # rnti_type
    "I"   # slot_idx
    "I"   # task_idx
    "I"   # sf_len
    "I"   # offset
    "f"   # snr_dB
    "q"   # full_secs
    "d"   # frac_secs
    "d"   # time_diff
    "I"   # nof_prb
    "I"   # start_symbol
    "I"   # nof_symbol
    f"{SRSRAN_MAX_PRB_NR}s"  # prb_map
)
Uplink_API_HDR_SIZE = struct.calcsize(Uplink_API_HDR_FMT)

ctx = zmq.Context()
sock = ctx.socket(zmq.SUB)
sock.connect(ZMQ_ADDR)
sock.setsockopt(zmq.SUBSCRIBE, b"")

TARGET_RNTI = 0x4601

while True:
    frames = sock.recv_multipart()
    hdr = struct.unpack(Uplink_API_HDR_FMT, frames[0][:Uplink_API_HDR_SIZE])
    if hdr[1] != TARGET_RNTI:
        continue
    # Process only messages for the target UE
    print(f"Slot: {hdr[3]}, SNR: {hdr[7]:.1f} dB")
```

---

## API Reference

The [`receiver.py`](receiver.py) module provides two functions:

### `receive(socket)`

Receives and unpacks a single uplink API message from the given ZMQ socket.

**Parameters:**

| Parameter | Type | Description |
|---|---|---|
| `socket` | `zmq.Socket` | A connected ZMQ SUB socket |

**Returns:** A `dict` with the following keys, or `None` if the message is malformed (not exactly 3 frames):

| Key | Type | Description |
|---|---|---|
| `message_type` | `int` | Message type (always `1`) |
| `rnti` | `int` | UE Radio Network Temporary Identifier |
| `rnti_type` | `int` | RNTI type code |
| `slot_idx` | `int` | Slot index |
| `task_idx` | `int` | Internal task index |
| `sf_len` | `int` | Subframe length in samples |
| `offset` | `int` | Timing advance in samples |
| `snr_dB` | `float` | Measured SNR in dB |
| `full_secs` | `int` | TOA integer seconds |
| `frac_secs` | `float` | TOA fractional seconds |
| `time_diff` | `float` | Propagation delay estimate (seconds) |
| `nof_prb` | `int` | Number of PRBs |
| `start_symbol` | `int` | Start OFDM symbol |
| `nof_symbol` | `int` | Number of OFDM symbols |
| `prb_map` | `tuple(numpy.ndarray,)` | Per-PRB allocation as a uint8 array (wrapped in a trailing-comma tuple) |
| `ul_buffer` | `numpy.ndarray` | Current subframe IQ samples (`complex64`) |
| `last_ul_buffer` | `numpy.ndarray` | Previous subframe IQ samples (`complex64`) |

### `show(msg)`

Prints a human-readable summary of an uplink API message to stdout.

**Parameters:**

| Parameter | Type | Description |
|---|---|---|
| `msg` | `dict` | A message dict as returned by `receive()` |

**Returns:** `None`. Output is printed to stdout and includes RNTI, slot index, timing, SNR, PRB allocation, and more.

---

## Testing

You can test the Python receiver without running the full Sni5Gect stack by using the C++ test sender located at [`tests/api_sender_test.cc`](tests/api_sender_test.cc).

### Building the test sender

Build the test sender as part of the normal CMake build process (it is included in the `shadower/comp/apis/tests/` directory).

### Running

1. **Start the Python receiver** in one terminal:

   ```bash
   python3 shadower/comp/apis/receiver.py
   ```

2. **Run the test sender** in another terminal:

   ```bash
   ./build/shadower/comp/apis/tests/api_sender_test
   ```

   The test sender will send 100 uplink messages at 1-second intervals using sample PUSCH IQ data from `shadower/test/data/srsran-n78-20MHz/`.

### Important: ZMQ address mismatch

The test sender binds to a **different IPC path** than the main Sni5Gect application:

| Component | ZMQ Address |
|---|---|
| Main Sni5Gect app | `ipc:///tmp/sni5gect-ul-api.zmq` |
| Test sender | `ipc:///tmp/sni5gect-api.zmq` |

When testing with the test sender, update the `ZMQ_ADDR` in `receiver.py` to match:

```python
ZMQ_ADDR = "ipc:///tmp/sni5gect-api.zmq"  # for use with the test sender
```
