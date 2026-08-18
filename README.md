# ESP32-P4 Ethernet (DHCPv4 & Link Management) Sample

This sample application demonstrates **100BASE-TX Ethernet networking via RMII** on the **ESP32-P4 Function EV Board** (ESP32-P4 v1.0 / v1.3 ECO2 Pre-silicon engineering sample) using Zephyr RTOS.

---

## Hardware Compatibility

- **Target Board**: ESP32-P4 Function EV Board (`esp32p4_function_ev_board/esp32p4/hpcore`)
- **Silicon Version**: ESP32-P4 v1.0 / v1.3 (ECO2 Sample)
- **PHY Transceiver**: IP101GR (Address: 1, Interface: RMII 50MHz Ref Clock)
- **Console / Flashing Port**: `/dev/ttyUSB0` (115200 baud)

---

## Workspace Prerequisites

Before building this sample, ensure your Zephyr workspace is set up and patched for ESP32-P4 v1.0:

1. **Bootstrap Workspace**:
   ```bash
   mkdir -p ~/zephyr-workspace && cd ~/zephyr-workspace
   git clone -b esp32p4-v1.0-sample https://github.com/ngtkien/zephyr-bootstrap.git
   ./zephyr-bootstrap/setup-zephyr.sh
   ```

2. **Apply Hardware Patches**:
   ```bash
   git clone https://github.com/ngtkien/esp32p4-v1-dev-kit.git
   cd zephyrproject/zephyr
   git apply ../../esp32p4-v1-dev-kit/patches/0001-esp32p4-function-ev-v1.0-soc-support.patch
   git apply ../../esp32p4-v1-dev-kit/patches/0002-esp32p4-function-ev-v1.0-ethernet-support.patch
   ```

---

## Quick Start

### 1. Build the Application
```bash
./build.sh
```

### 2. Flash to ESP32-P4 & Monitor
```bash
./flash.sh /dev/ttyUSB0
```

### 3. Automated End-to-End Test Suite
```bash
./test.sh /dev/ttyUSB0
```

---

## Documentation & Architecture

- [OSI_DEMO.md](OSI_DEMO.md): 7-Layer OSI Model demonstration, packet encapsulation/decapsulation details, and DORA handshake.
- [NETWORK_ARCHITECTURE.md](NETWORK_ARCHITECTURE.md): Software architecture of the Zephyr Network Stack and hardware mapping on the ESP32-P4 Function EV Board.
