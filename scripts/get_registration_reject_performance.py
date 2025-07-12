import os
import argparse

parser = argparse.ArgumentParser(description="Get sniffing performance from pcap files.")
parser.add_argument("-s", type=str, default=f"{os.getcwd()}/logs", help="Path to logs file directory")
args = parser.parse_args()

threshold = 4
def parse_pcap(path):
    summary_path = path + ".summary"
    if not os.path.exists(summary_path):
        command = f"tshark -r {path} -T text > {summary_path}"
        os.system(command)
    with open(summary_path, "r") as f:
        lines = f.read().splitlines()
    valid = False
    sec_mod_cmd = 0
    authentication_req = 0
    identity_request = 0
    registration_acpt = 0
    success = False
    for line in lines:
        if "RRC Setup Complete, Registration request" in line:
            valid = True
        if "Security Mode Command" in line:
            sec_mod_cmd += 1
            if sec_mod_cmd > threshold:
                success = True
        if "Authentication request" in line:
            authentication_req += 1
            if authentication_req > threshold:
                success = True
        if "Identity request" in line:
            identity_request += 1
            if identity_request > threshold:
                success = True
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
