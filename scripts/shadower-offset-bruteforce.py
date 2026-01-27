from runner import start_new_test

def update_testcases(test_index):
    with open("configs/srsran-n5-10MHz-b210.yaml", "r") as f:
        config_lines = f.readlines()
    with open("configs/srsran-n5-10MHz-b210.yaml", "w") as f:
        for line in config_lines:
            if "ul_advancement:" in line:
                f.write(f"  ul_advancement: {test_index}\n")
            else:
                f.write(line)

if __name__ == "__main__":
    for offset in range(220, 250, 1):
        print(f"Starting test with offset {offset}...")
        start_new_test(
            test_index=offset,
            update_testcases=update_testcases,
            phone_id="3B6F5VE8GCL372LV",
            toggle_count=3,
            toggle_sleep=5,
            enable_logcat=False,
        )
