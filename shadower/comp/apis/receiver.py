import zmq
import struct
import numpy as np
ZMQ_ADDR = "ipc:///tmp/sni5gect-ul-api.zmq"

SRSRAN_MAX_PRB_NR = 275  # adjust to match your buippld
CF_T_DTYPE = np.complex64  # cf_t = complex<float>

Uplink_API_HDR_FMT = (
    "<"     # little endian
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
Uplink_API_HDR_SIZE_NO_PAD = struct.calcsize(Uplink_API_HDR_FMT)

def show(msg):
    print("Received uplink message for RNTI:", msg["rnti"], msg["rnti_type"])
    print("  Slot index:", msg["slot_idx"])
    print("  Task index:", msg["task_idx"])
    print("  SF length:", msg["sf_len"])
    print("  Offset:", msg["offset"])
    print("  SNR (dB):", msg["snr_dB"])
    print("  Time (secs):", msg["full_secs"], "+", msg["frac_secs"])
    print("  Time diff (secs):", msg["time_diff"], " Samples: ", msg["time_diff"] * 23.04e6)
    print("  No. of PRBs:", msg["nof_prb"])
    print("  Start symbol:", msg["start_symbol"])
    print("  No. of symbols:", msg["nof_symbol"])
    prb_str = ""
    num_prbs = msg["nof_prb"]
    prb_map = msg["prb_map"][0]
    for i in prb_map[:num_prbs]:
        prb_str += f"{i}"
    print("  UL PRB used:", prb_str)


def receive(socket):
    frames = socket.recv_multipart()
    if len(frames) != 3:
        print("Invalid message, expected 3 frames")
        return None
    hdr_buf, last_buf, buf = frames
    hdr = struct.unpack(Uplink_API_HDR_FMT, hdr_buf[:Uplink_API_HDR_SIZE_NO_PAD])
    result = {
        "message_type": hdr[0],
        "rnti": hdr[1],
        "rnti_type": hdr[2],
        "slot_idx": hdr[3],
        "task_idx": hdr[4],
        "sf_len": hdr[5],
        "offset": hdr[6],
        "snr_dB": hdr[7],
        "full_secs": hdr[8],
        "frac_secs": hdr[9],
        "time_diff": hdr[10],
        "nof_prb": hdr[11],
        "start_symbol": hdr[12],
        "nof_symbol": hdr[13],
    }
    prb_map = np.frombuffer(hdr[14], dtype=np.uint8)
    result["prb_map"] =  prb_map,
    ul_iq = np.frombuffer(buf, dtype=CF_T_DTYPE, count=result["sf_len"])
    result["ul_buffer"] =     ul_iq
    last_ul_iq = np.frombuffer(last_buf, dtype=CF_T_DTYPE, count=result["sf_len"])
    result["last_ul_buffer"] = last_ul_iq

    return result

if __name__ == "__main__":
    ctx = zmq.Context()
    sock = ctx.socket(zmq.SUB)
    sock.connect(ZMQ_ADDR)
    sock.setsockopt(zmq.SUBSCRIBE, b"")
    print("Listening for uplink API messages...")

    while True:
        try:
            result = receive(sock)
            show(result)
        except KeyboardInterrupt:
            print("Interrupted by user, exiting...")
            break
        # except Exception as e:
        #     print("Error receiving or showing message:", e)
