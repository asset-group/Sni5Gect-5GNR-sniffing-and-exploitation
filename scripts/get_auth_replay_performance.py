import os
import argparse

parser = argparse.ArgumentParser(description="Get sniffing performance from pcap files.")
parser.add_argument("-s", type=str, default=f"{os.getcwd()}/logs", help="Path to logs file directory")
args = parser.parse_args()

def parse_pcap(path):
    summary_path = path + ".summary"
    if not os.path.exists(summary_path):
        command = f"tshark -r {path} -T text > {summary_path}"
        os.system(command)
    with open(summary_path, "r") as f:
        lines = f.read().splitlines()
    valid = False
    success = False
    failure = False
    auth_failure = 0
    for line in lines:
        if "RRC Setup Complete, Registration request" in line:
            valid = True
        if "Authentication failure" in line:
            auth_failure += 1
            if auth_failure > 1:
                success = True
        if "Authentication response" in line:
            failure = True
        if "Security mode complete" in line:
            failure = True
        if "Registration complete" in line:
            failure = True
        if "UE Capability Enquiry" in line:
            failure = True
        if "Registration accept" in line or "Registration complete" in line:
            failure = True
    return success, valid, failure

total = 0
success_count = 0
files = os.listdir(args.s)
for file in files:
    if file.startswith("UE-") and file.endswith(".pcap"):
        path = os.path.join(os.getcwd(), args.s, file)
        s, v, fail = parse_pcap(path)
        if s and not fail:
            success_count += 1
        if v:
            total += 1

print(f"{success_count} / {total} = {success_count / total * 100:.2f}%")
