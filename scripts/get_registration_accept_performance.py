import os
import argparse

parser = argparse.ArgumentParser(
    description="Get registration accept performance from pcap files."
)
parser.add_argument(
    "-s", type=str, default=f"{os.getcwd()}/logs", help="Path to logs file directory"
)
args = parser.parse_args()

threshold = 4


def parse_pcap(path):
    summary_path = path + ".summary"
    if not os.path.exists(summary_path):
        command = f"tshark -r {path} -T text > {summary_path}"
        os.system(command)
    with open(summary_path, "r") as f:
        lines = f.read().splitlines()
    pdu_session_establishment = False
    pdu_session_establishment_line = None
    registration_acpt = False
    registration_acpt_line = None
    rrc_release = False
    success = False
    failed = False
    valid = False
    for i, line in enumerate(lines):
        if "UL Information Transfer, Registration complete" in line:
            registration_acpt = True
            registration_acpt_line = i
            valid = True
        if "PDU session establishment request" in line:
            pdu_session_establishment = True
            pdu_session_establishment_line = i
            valid = True
        if "RRC Release" in line:
            rrc_release = True
            if (
                i < 20
                and i - pdu_session_establishment_line < 15
                and i - registration_acpt_line < 15
            ):
                return True, True
            valid = True
        if "Security Mode Command" in line:
            valid = True
        if "Authentication request" in line:
            valid = True
        if "UE Capability Information" in line:
            valid = True
            failed = True
        if "Authentication response" in line:
            valid = True
            failed = True
        if "Security Mode Complete" in line:
            valid = True
            failed = True
        if "UE Capability Information" in line:
            valid = True
            failed = True
        if "RRC Reconfiguration" in line:
            valid = True
            failed = True
        if "RRC Reconfiguration Complete" in line:
            valid = True
            failed = True
    if failed:
        success = False
    return success, valid


total = 0
success_count = 0
files = os.listdir(args.s)
for file in files:
    if file.startswith("UE-") and file.endswith(".pcap"):
        path = os.path.join(os.getcwd(), args.s, file)
        s, v = parse_pcap(path)
        if s:
            success_count += 1
        if v:
            total += 1

print(f"{success_count} / {total} = {success_count / total * 100:.2f}%")
