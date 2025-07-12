import os
import argparse

parser = argparse.ArgumentParser(description="Get sniffing performance from pcap files.")
parser.add_argument("-s", type=str, default=f"{os.getcwd()}/logs", help="Path to logs file directory")
parser.add_argument("-g", type=str, default=f"{os.getcwd()}/logs/sni5gect.log", help="Path to sni5gect log file")
args = parser.parse_args()

new_connection = "RRC Setup Complete, Registration request"
success_resp = "Identity: "

with open(args.g, 'r') as f:
    lines = f.read().splitlines()

total_connection = 0
success_count = 0
for line in lines:
    if new_connection in line:
        total_connection += 1
    if success_resp in line:
        success_count += 1

print(f"{success_count} / {total_connection} = {success_count / total_connection * 100:.2f}%")
