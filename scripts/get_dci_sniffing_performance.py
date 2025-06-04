import re
import pandas as pd


def parse_to_chunks(file):
    with open(file, "r") as f:
        lines = f.readlines()

    chunks = []
    re_timestamp = re.compile(r"^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}\.\d{6}")
    for n, line in enumerate(lines, 1):
        match = re_timestamp.match(line)
        if match:
            chunks.append([])
        chunks[-1].append((n, line.strip()))
    return chunks


def parse_gnb_logs(file):
    chunks = parse_to_chunks(file)
    df = pd.DataFrame(
        columns=["Line", "Slot", "RNTI", "Direction", "Format", "MCS", "Released"]
    )
    ue_status = {}
    total_dl = 0
    total_ul = 0
    for chunk in chunks:
        slot_number = None
        for n, line in chunk:
            # Detect new UE by searching for tc-rnti
            match = re.search(r"tc-rnti=(0x[0-9a-fA-F]+)", line)
            if match:
                rnti = match.group(1)
                ue_id = int(rnti, 16)
                if ue_id not in ue_status:
                    ue_status[ue_id] = False

            # Detect the UE get released
            match = re.search(r"rnti=(0x[0-9a-fA-F]+): Containerized rrcRelease", line)
            if match:
                rnti = match.group(1)
                ue_id = int(rnti, 16)
                ue_status[ue_id] = True

            # Detect the slot decision line
            match = re.search(r"\[(\s+\d+\.\d+)\] Slot decisions", line)
            if match:
                slot_number_str = match.group(1)
                frame_number = int(slot_number_str.split(".")[0])
                slot_idx = int(slot_number_str.split(".")[1])
                slot_number = frame_number * 20 + slot_idx

            # Detect the PDCCH line from slot decision chunk
            match = re.search(
                r"- (\w{2,}) PDCCH: rnti=(0x[0-9a-fA-F]+).*?format=([\d_]+).*?mcs=(\d+)",
                line,
            )
            if "DL PDCCH:" in line and not match:
                print(f"Ignored {line}")
            if match:
                direction = match.group(1)
                rnti = match.group(2)
                format = match.group(3)
                mcs = match.group(4)
                assert (
                    slot_number is not None
                ), "Slot number should be detected before PDCCH line"
                df = pd.concat(
                    [
                        df,
                        pd.DataFrame(
                            [
                                [
                                    n,
                                    slot_number,
                                    rnti,
                                    direction,
                                    format,
                                    mcs,
                                    ue_status.get(int(rnti, 16)),
                                ]
                            ],
                            columns=df.columns,
                        ),
                    ],
                    ignore_index=True,
                )
                if direction == "UL":
                    total_ul += 1
                elif direction == "DL":
                    total_dl += 1
    print(f"gNB log: Total DL: {total_dl}  Total UL: {total_ul}")
    return df


def parse_sni5gect_logs(file):
    df = pd.DataFrame(
        columns=["Line", "Slot", "RNTI", "Direction", "DCI", "MCS", "Released"]
    )
    with open(file, "r") as f:
        lines = f.readlines()
    for n, line in enumerate(lines):
        match = re.search(r"\[S:(\d+)\].*?RRC Release", line)
        if match:
            last_3 = df.tail(3)
            if (last_3["Slot"] == match.group(1)).any():
                index_to_update = last_3[last_3["Slot"] == match.group(1)].index
                df.loc[index_to_update, "Released"] = True
        match = re.search(
            r"DCI (\w+) slot \d+ (\d+): c-rnti=(0x[0-9a-fA-F]+) dci=(\w+) ss.*?mcs=(\d+)",
            line,
        )
        if match:
            direction = match.group(1)
            slot_number = match.group(2)
            rnti = match.group(3)
            dci = match.group(4)
            mcs = match.group(5)
            df = pd.concat(
                [
                    df,
                    pd.DataFrame(
                        [[n, slot_number, rnti, direction, dci, mcs, False]],
                        columns=df.columns,
                    ),
                ],
                ignore_index=True,
            )
    return df

def compare_sni5gect(df, gnb_df):
    # From srsRAN
    gnb_ul_df = gnb_df[(gnb_df["Direction"] == "UL")]
    gnb_dl_df = gnb_df[(gnb_df["Direction"] == "DL")]
    # From the framework
    ul_df = df[(df["Direction"] == "UL")]
    dl_df = df[(df["Direction"] == "DL")]
    return {
        "gnb_dl": len(gnb_dl_df),
        "gnb_ul": len(gnb_ul_df),
        "sni5gect_dl": len(dl_df),
        "sni5gect_ul": len(ul_df),
        "dl_success_rate": len(dl_df) / len(gnb_dl_df),
        "ul_success_rate": len(ul_df) / len(gnb_ul_df),
    }

if __name__ == "__main__":
    import argparse
    import os
    
    parser = argparse.ArgumentParser(description="Get sniffing performance from logs.")
    parser.add_argument(
        "-s", type=str, default=f"{os.getcwd()}/logs/sni5gect.log", help="Path to Sni5Gect log file"
    )
    parser.add_argument(
        "-g", type=str, default=f"{os.getcwd()}/logs/gnb.log", help="Path to gNB log file"
    )
    args = parser.parse_args()

    sni5gect_df = parse_sni5gect_logs(args.s)
    gnb_df = parse_gnb_logs(args.g)

    result = compare_sni5gect(sni5gect_df, gnb_df)
    print(f"DL Success rate: {result['sni5gect_dl']} / {result['gnb_dl']} ({result['dl_success_rate']:.2%})")
    print(f"UL Success rate: {result['sni5gect_ul']} / {result['gnb_ul']} ({result['ul_success_rate']:.2%})")
    print(f"Total: {result['sni5gect_dl'] + result['sni5gect_ul']} / {result['gnb_dl'] + result['gnb_ul']} ({(result['sni5gect_dl'] + result['sni5gect_ul']) / (result['gnb_dl'] + result['gnb_ul']):.2%})")
