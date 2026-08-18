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

From the `esp32p4-ethernet` directory:

```bash
./build.sh
```

*(This automatically sources `zephyr-env.sh` and compiles the Sysbuild multi-image bundle: MCUboot + signed application)*

### 2. Flash to ESP32-P4 & Monitor

Ensure your board is connected via USB (e.g. `/dev/ttyUSB0`):

```bash
./flash.sh /dev/ttyUSB0
```

### 3. Automated End-to-End Test

To run the complete automated build, flash, and UART milestone verification suite in one command:

```bash
./test.sh /dev/ttyUSB0
```

---

## Testing & Verifying Ethernet

1. **Plug an RJ45 Ethernet cable** from your router/switch into the board's Ethernet jack.
2. Watch the serial monitor. You should see:
   ```text
   [00:00:01.234,000] <inf> esp32p4_ethernet: >> Ethernet Cable Connected (Carrier ON)
   [00:00:02.100,000] <inf> esp32p4_ethernet: ========================================
   [00:00:02.100,000] <inf> esp32p4_ethernet:  Ethernet IPv4 Lease Acquired!
   [00:00:02.100,000] <inf> esp32p4_ethernet:  IP Address: 192.168.1.150
   [00:00:02.100,000] <inf> esp32p4_ethernet:  Netmask:    255.255.255.0
   [00:00:02.100,000] <inf> esp32p4_ethernet:  Gateway:    192.168.1.1
   [00:00:02.100,000] <inf> esp32p4_ethernet:  Lease Time: 86400 seconds
   [00:00:02.100,000] <inf> esp32p4_ethernet: ========================================
   ```

3. **Use the Interactive Zephyr Shell**:
   - Ping your gateway or Google DNS:
     ```text
     uart:~$ net ping 8.8.8.8
     ```
   - Show network interface status:
     ```text
     uart:~$ net iface
     ```
   - Show packet transmission statistics:
     ```text
     uart:~$ net stats
     ```
