import os
import shutil
import subprocess
from time import sleep

def kill_shadower():
    os.system("sudo pkill -9 shadower")

def run_shadower(offset, index):
    # Kill existing shadower if it is running
    kill_shadower()
    sleep(1)

    p = subprocess.Popen(['script', '-c', './build/shadower/shadower configs/srsran-n5-10MHz-b210.yaml', '-O', 'logs/shadower.log', '--flush'])
    sleep(30)
    p.terminate()
    kill_shadower()
    shutil.move('logs/shadower.log', f'results/shadower-run_{offset}_{index}.log')

def update_config(offset):
    with open("configs/srsran-n5-10MHz-b210.yaml", "r") as f:
        config_lines = f.readlines()
    is_first = True
    with open("configs/srsran-n5-10MHz-b210.yaml", "w") as f:
        for line in config_lines:
            if is_first and "rx_offset:" in line:
                f.write(f"      rx_offset: {offset}\n")
                is_first = False
            else:
                f.write(line)

if __name__ == "__main__":
    if not os.path.exists("results"):
        os.makedirs("results")
    for offset in range(-3000, 3000, 100):
        update_config(offset)
        for index in range(3):
            print(f"Running shadower with rx_offset {offset}. {index}..")
            run_shadower(offset, index)