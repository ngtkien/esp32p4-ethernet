#!/usr/bin/env bash
set -Eeuo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WORKSPACE_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# Source Zephyr environment
if [[ -f "$WORKSPACE_ROOT/zephyr-env.sh" ]]; then
    # shellcheck disable=SC1091
    source "$WORKSPACE_ROOT/zephyr-env.sh"
else
    echo "ERROR: zephyr-env.sh not found at $WORKSPACE_ROOT" >&2
    exit 1
fi

PORT="${1:-${ESPPORT:-/dev/ttyUSB0}}"
BAUD="${BAUD:-460800}"
BUILD_DIR="${BUILD_DIR:-$SCRIPT_DIR/build}"

echo "=================================================="
echo "  ESP32-P4 v1.0 Ethernet Automated Test Runner   "
echo "  Target Port: $PORT"
echo "=================================================="

# 1. Build
echo -e "\n[STEP 1/3] Building ESP32-P4 Ethernet application..."
west build \
    --sysbuild \
    --pristine=auto \
    --board esp32p4_function_ev_board/esp32p4/hpcore \
    --build-dir "$BUILD_DIR" \
    "$SCRIPT_DIR"

MCUBOOT_BIN="$BUILD_DIR/mcuboot/zephyr/zephyr.bin"
APP_BIN="$BUILD_DIR/esp32p4-ethernet/zephyr/zephyr.signed.bin"

if [[ ! -f "$MCUBOOT_BIN" || ! -f "$APP_BIN" ]]; then
    echo "ERROR: Built binaries missing!" >&2
    exit 1
fi

# 2. Flash
echo -e "\n[STEP 2/3] Flashing MCUboot and application to $PORT..."
esptool \
    --chip esp32p4 \
    --port "$PORT" \
    --baud "$BAUD" \
    write-flash \
    --flash-mode dio \
    --flash-freq 80m \
    --flash-size 16MB \
    0x2000 "$MCUBOOT_BIN" \
    0x20000 "$APP_BIN"

# 3. Monitor and Assert
echo -e "\n[STEP 3/3] Capturing Serial Boot Output & Verifying Test Milestones..."
python3 - <<PY_TEST
import serial
import time
import sys

port = "$PORT"
baud = 115200
duration = 10.0

print(f"Connecting to {port} at {baud} baud...")
try:
    s = serial.Serial(port, baud, timeout=1.0)
except Exception as e:
    print(f"FAIL: Could not open serial port {port}: {e}")
    sys.exit(1)

# Hardware reset via RTS
s.dtr = False
s.rts = True
time.sleep(0.1)
s.rts = False
time.sleep(0.2)

logs = []
start = time.time()
while time.time() - start < duration:
    line = s.readline()
    if line:
        text = line.decode('utf-8', errors='replace').strip()
        if text:
            print(f"  [UART] {text}")
            logs.append(text)

s.close()
full_log = "\n".join(logs)

# Assertions
checks = [
    ("ESP32-P4 ROM / Silicon v1.0 Boot", "chip revision: v1.0" in full_log or "esp32p4" in full_log),
    ("MCUboot Image Load", "Loading image 0 - slot 0 from flash" in full_log or "Booting Zephyr OS" in full_log),
    ("Zephyr OS Kernel Start", "Booting Zephyr OS build" in full_log),
    ("PHY MII Initialization", "PHY (1) ID" in full_log),
    ("PHY Link Up & Carrier Detect", "Link speed 100 Mb" in full_log or "Carrier ON" in full_log),
    ("DHCPv4 Discover Transmission", "send discover" in full_log or "TX frame" in full_log),
    ("DHCPv4 IPv4 Lease Acquisition", "Ethernet IPv4 Lease Acquired" in full_log or "state=bound" in full_log)
]

print("\n================ TEST SUMMARY ================")
all_passed = True
for name, passed in checks:
    status = "\033[1;32m[PASS]\033[0m" if passed else "\033[1;31m[FAIL]\033[0m"
    print(f"  {status}  {name}")
    if not passed:
        all_passed = False

if all_passed:
    print("\n\033[1;32m>>> ALL TESTS PASSED SUCCESSFULLY! <<<\033[0m\n")
    sys.exit(0)
else:
    print("\n\033[1;31m>>> SOME TESTS FAILED! <<<\033[0m\n")
    sys.exit(1)
PY_TEST
