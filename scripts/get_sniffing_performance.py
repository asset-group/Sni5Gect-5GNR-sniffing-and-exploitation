import os
import json
import pandas as pd


def get_specific_field_from_json(packet, field):
    for k, v in packet.items():
        if k == field:
            return v
        elif isinstance(v, dict):
            result = get_specific_field_from_json(v, field)
            if result is not None:
                return result
    return None


def parse_pcap_raw(pcap_file):
    json_path = pcap_file + ".jsonraw"
    if not os.path.exists(json_path):
        command = f"tshark -r {pcap_file} -T jsonraw > {json_path}"
        os.system(command)
    with open(json_path) as f:
        try:
            packet_list = json.load(f)
        except:
            print(f"Error loading JSON file: {json_path}")
            return []
    result = []
    for p in packet_list:
        mac_nr_raw = get_specific_field_from_json(p, "mac-nr_raw")
        if mac_nr_raw is None or len(mac_nr_raw) < 1:
            result.append("")
        result.append(mac_nr_raw[0])
    return result


def parse_rnti_msin_from_pcap(pcap_file):
    json_path = pcap_file + ".json"
    if not os.path.exists(json_path):
        command = f"tshark -r {pcap_file} -T json > {json_path}"
        os.system(command)
    with open(json_path) as f:
        try:
            packet_list = json.load(f)
        except:
            print(f"Error loading JSON file: {json_path}")
            return [], {}
    rnti_to_msin = {}
    rnti_list = []
    for p in packet_list:
        rnti = get_specific_field_from_json(p, "mac-nr.rnti")
        msin = get_specific_field_from_json(p, "nas_5gs.mm.suci.msin")
        rnti = int(rnti, 16)
        if msin is not None:
            rnti_to_msin[rnti] = msin
        rnti_list.append(rnti)
    return rnti_list, rnti_to_msin


def parse_direction_from_pcap(pcap_file):
    json_path = pcap_file + ".json"
    if not os.path.exists(json_path):
        command = f"tshark -r {pcap_file} -T json > {json_path}"
        os.system(command)
    with open(json_path) as f:
        try:
            packet_list = json.load(f)
        except:
            print(f"Error loading JSON file: {json_path}")
            return []
    direction_list = []
    for p in packet_list:
        direction = get_specific_field_from_json(p, "mac-nr.direction")
        direction_list.append(direction)
    return direction_list


def parse_sni5gect_pcap(pcap_file, rnti: int):
    print(f"Parsing Sni5Gect pcap file: {pcap_file}")
    packet_raw = parse_pcap_raw(pcap_file)
    direction_list = parse_direction_from_pcap(pcap_file)
    df = pd.DataFrame({"mac-nr": packet_raw, "direction": direction_list, "rnti": rnti})
    return df


def parse_gnb_pcap(pcap_file):
    packet_raw = parse_pcap_raw(pcap_file)
    direction_list = parse_direction_from_pcap(pcap_file)
    rnti_list, rnti_to_msin = parse_rnti_msin_from_pcap(pcap_file)
    df = pd.DataFrame(
        {"mac-nr": packet_raw, "direction": direction_list, "rnti": rnti_list}
    )
    return df, list(set(rnti_list)), rnti_to_msin


def compare_single_ue_with_gnb_pcap(sni5gect_df, gnb_df, rnti: int):
    gnb_rnti_df = gnb_df[gnb_df["rnti"] == rnti]
    gnb_rnti_dl = gnb_rnti_df[gnb_rnti_df["direction"] == "1"]
    gnb_rnti_ul = gnb_rnti_df[gnb_rnti_df["direction"] == "0"]

    df_matched = gnb_rnti_df[gnb_rnti_df["mac-nr"].isin(sni5gect_df["mac-nr"])]

    sni5gect_dl = df_matched[df_matched["direction"] == "1"]
    sni5gect_ul = df_matched[df_matched["direction"] == "0"]

    result = {
        "rnti": rnti,
        "sni5gect_dl": len(sni5gect_dl),
        "sni5gect_ul": len(sni5gect_ul),
        "gnb_dl": len(gnb_rnti_dl),
        "gnb_ul": len(gnb_rnti_ul),
    }
    return result

if __name__ == "__main__":
    import argparse
    import os
    parser = argparse.ArgumentParser(description="Get sniffing performance from pcap files.")
    parser.add_argument("-s", type=str, default=f"{os.getcwd()}/logs", help="Path to Sni5Gect pcap file directory")
    parser.add_argument("-g", type=str, default=f"{os.getcwd()}/logs/gnb_mac.pcap", help="Path to gNB pcap file")
    args = parser.parse_args()
    gnb_df, rnti_list, rnti_to_msin = parse_gnb_pcap(args.g)
    for rnti in rnti_list:
        if rnti < 17921 or rnti > 60000:
            continue
        sni5gect_df = parse_sni5gect_pcap(os.path.join(args.s, f"UE-{rnti}.pcap"), rnti=rnti)
        ue_result = compare_single_ue_with_gnb_pcap(sni5gect_df, gnb_df, rnti)
        ue_result["dl_success_rate"] = (
            ue_result["sni5gect_dl"] / ue_result["gnb_dl"] if ue_result["gnb_dl"] > 0 else 0
        )
        ue_result["ul_success_rate"] = (
            ue_result["sni5gect_ul"] / ue_result["gnb_ul"] if ue_result["gnb_ul"] > 0 else 0
        )
        print(f"RNTI: {rnti} MSIN: {rnti_to_msin.get(rnti)}:\n"
            f"DL Success rate: {ue_result['sni5gect_dl']} / {ue_result['gnb_dl']} ({ue_result['dl_success_rate']:.2%})\n"
            f"UL Success rate: {ue_result['sni5gect_ul']} / {ue_result['gnb_ul']} ({ue_result['ul_success_rate']:.2%})")