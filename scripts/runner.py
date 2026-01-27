#!/usr/bin/python3
import os
import sys
import shutil
from time import sleep
import libtmux
from loguru import logger
from pathlib import Path
from libtmux.constants import PaneDirection

SRSRAN_CONFIG_FILE = "/etc/srsran/srsran-n5-10MHz.yml"
SESSION_NAME = "sni5gect"
RESULT_DIR = "results"


def get_or_create_session(session_name):
    session = None
    # Check if the session with name effnet is running, else create new session
    server = libtmux.Server()
    for sess in server.sessions:
        if sess.session_name == session_name:
            session = sess
            logger.info(
                f"Session with name {session_name} exists, use existing session"
            )
    if session is None:
        session = server.new_session(session_name=session_name, attach=False)
        logger.info(f"Creating new session with name: {session_name}")
    return session


def init_logger():
    # Remove default logger
    logger.remove()
    # Ensure log directory exists
    log_path = Path("logs/runner.log")
    log_path.parent.mkdir(parents=True, exist_ok=True)

    logger.add(sys.stdout, level="INFO", enqueue=True)
    # Add file logger
    logger.add(
        str(log_path),
        level="INFO",
        format=(
            "{time:YYYY-MM-DD HH:mm:ss.SSS} | "
            "{level: <8} | "
            "{name}:{function}:{line} - {message}"
        ),
        enqueue=True,
    )


def kill_open5gs():
    os.system("sudo pkill -9 open5gs")
    os.system("sudo pkill -9 app")


def start_open5gs(core_pane):
    # Kill existing open5gs if it is running
    kill_open5gs()
    sleep(1)

    # Start open5gs in the core panel
    core_pane.send_keys(f"cd /root/open5gs")
    sleep(1)
    core_pane.send_keys(f"./build/tests/app/5gc -c open5gs.yaml")
    logger.info("Started core network")


def kill_shadower():
    os.system("sudo pkill -9 shadower")

def start_shadower(shadower_pane):
    # Kill existing shadower if it is running
    kill_shadower()
    sleep(1)

    # Start shadower in the shadower panel
    shadower_pane.send_keys(f"cd /root/sni5gect")
    sleep(1)
    shadower_pane.send_keys(
        f'script -c "./build/shadower/shadower configs/srsran-n5-10MHz-b210.yaml" -O logs/shadower.log --flush'
    )
    logger.info("Started shadower")

def wait_for_shadower():
    shadower_log_file = "/root/sni5gect/logs/shadower.log"
    for i in range(6):
        if not os.path.exists(shadower_log_file):
            sleep(2)
            continue
        with open(shadower_log_file, "r") as f:
            for line in f.readlines():
                if "SIB1 applied to all workers" in line:
                    logger.info("Shadower started successfully")
                    return True

        logger.info(f"Shadower not ready {i}/6")
        sleep(2)
    return False


def kill_srsran():
    os.system("sudo pkill -9 gnb")


def start_srsran(ran_pane):
    # Kill existing srsran if it is running
    kill_srsran()
    sleep(1)

    ran_pane.send_keys(f"cd /root/srsran")
    sleep(1)
    # Start srsran in the ran panel
    ran_pane.send_keys(
        f'script -c "./build/apps/gnb/gnb -c {SRSRAN_CONFIG_FILE}" -O logs/srsran.log --flush'
    )
    logger.info("Started srsran gNB")


def wait_for_srsran():
    srsran_log_file = "/root/srsran/logs/srsran.log"
    for i in range(3):
        if not os.path.exists(srsran_log_file):
            sleep(2)
            continue
        with open(srsran_log_file, "r") as f:
            for line in f.readlines():
                if "==== gNB started ===" in line:
                    logger.info("srsran gNB started successfully")
                    return True

        logger.info(f"srsRAN not ready {i}/3")
        sleep(2)
    return False


def turn_on_airplane_mode(adb_id):
    os.system(f"adb -s {adb_id} shell cmd connectivity airplane-mode enable")
    logger.info("Airplane mode enabled")


def turn_off_airplane_mode(adb_id):
    os.system(f"adb -s {adb_id} shell cmd connectivity airplane-mode disable")
    logger.info("Airplane mode disabled")


def kill_logcat(adb_id):
    os.system(f'sudo pkill -f "adb -s {adb_id} logcat -b radio,crash,system,main"')
    logger.info("Killed existing logcat processes")


def logcat_clear(adb_id):
    os.system(f"adb -s {adb_id} logcat -c")
    logger.info("Cleared logcat")


def logcat_dump(adb_id, logcat_pane):
    kill_logcat(adb_id)
    logcat_clear(adb_id)
    sleep(1)
    logcat_pane.send_keys(
        f"adb -s {adb_id} logcat -b radio,crash,system,main > logs/logcat_{adb_id}.log"
    )
    logger.info("Started logcat dump")


def start_new_test(
    test_index,
    update_testcases,
    phone_id,
    toggle_count=3,
    toggle_sleep=10,
    enable_logcat=False,
):
    # Create the logs directory if it doesn't exist
    if not os.path.exists("logs"):
        os.makedirs("logs")
    else:
        shutil.rmtree("logs")
        os.makedirs("logs")

    if not os.path.exists("/root/srsran/logs"):
        os.makedirs("/root/srsran/logs")
    if not os.path.exists("/root/open5gs/logs"):
        os.makedirs("/root/open5gs/logs")

    # Re-initialize logger
    init_logger()

    # Get the session
    session = get_or_create_session(SESSION_NAME)
    window = session.active_window
    for pane in window.panes[1:]:
        pane.kill()

    # Turn on airplane mode on the phone
    turn_on_airplane_mode(phone_id)

    # Split the window into two panes
    old_pane = window.active_pane
    core_pane = old_pane.split(direction=PaneDirection.Below)
    old_pane.kill()
    ran_pane = core_pane.split(direction=PaneDirection.Right)
    shadower_pane = ran_pane.split(direction=PaneDirection.Below)
    if enable_logcat:
        logcat_pane = ran_pane.split(direction=PaneDirection.Below)

    # Start the core network first
    start_open5gs(core_pane)

    # Then start the srsran
    start_srsran(ran_pane)
    start_shadower(shadower_pane)
    sleep(3)
    if enable_logcat:
        logcat_dump(phone_id, logcat_pane)

    # Wait until srsRAN is ready
    if not wait_for_srsran():
        logger.error("srsRAN gNB failed to start")
        return
    if not wait_for_shadower():
        logger.error("Shadower failed to start")
        return
    logger.info("Base station is ready, waiting for UE connection")

    # Update the testcases
    update_testcases(test_index)

    for i in range(toggle_count):
        logger.info(f"Toggle count {i+1}/{toggle_count}")
        turn_off_airplane_mode(phone_id)
        sleep(toggle_sleep)
        turn_on_airplane_mode(phone_id)
        sleep(1)

    ran_pane.send_keys("q", enter=True)
    core_pane.send_keys("C-c", enter=True)
    shadower_pane.send_keys("C-c", enter=True)
    sleep(3)
    if enable_logcat:
        kill_logcat(phone_id)
    sleep(1)
    if not os.path.exists(RESULT_DIR):
        os.makedirs(RESULT_DIR)
    if not os.path.exists(f"{RESULT_DIR}/test-{test_index}"):
        os.makedirs(f"{RESULT_DIR}/test-{test_index}")
    logger.info(f"Saved logs to {RESULT_DIR}/test-{test_index}")
    shutil.move("/root/open5gs/logs/open5gs.log", f"{RESULT_DIR}/test-{test_index}/open5gs.log")
    shutil.move("/root/srsran/logs/gnb.log", f"{RESULT_DIR}/test-{test_index}/gnb.log")
    shutil.move("/root/srsran/logs/gnb_mac.pcap", f"{RESULT_DIR}/test-{test_index}/gnb_mac.pcap")
    shutil.move("/root/sni5gect/logs/shadower.log", f"{RESULT_DIR}/test-{test_index}/shadower.log")
    shutil.move(f"logs/runner.log", f"{RESULT_DIR}/test-{test_index}/runner.log")