import os


def parse_pcap(pcap_file):
    summary_path = pcap_file + ".summary"
    if not os.path.exists(summary_path):
        command = f"tshark -r {pcap_file} -T text > {summary_path}"
        os.system(command)
    with open(summary_path) as f:
        summaries = f.readlines()
    connection_count = 0
    success_count = 0
    last_line = ""
    for i, line in enumerate(summaries):
        if "RRC Setup Complete, Registration request" in line:
            connection_count += 1
        elif "Registration reject (N1 mode not allowed)" in line:
            if "RRC Setup Complete, Registration request" in last_line:
                success_count += 1
        last_line = line
    return connection_count, success_count

if __name__ == "__main__":
    import argparse
    import os
    parser = argparse.ArgumentParser(description="Get sniffing performance from pcap files.")
    parser.add_argument("-g", type=str, default=f"{os.getcwd()}/logs/qcsuper.pcap", help="Path to qcsuper pcap file")
    args = parser.parse_args()
    conn_count, success_count = parse_pcap(args.g)
    if conn_count == 0:
        print("0/0=0%")
    else:
        print(f"{success_count} / {conn_count} = {success_count / conn_count * 100:.2f}%")
