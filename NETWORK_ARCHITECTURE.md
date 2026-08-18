# Zephyr Network Stack & ESP32-P4 Hardware Architecture

This document illustrates the software architecture of the Zephyr RTOS Network Stack and its concrete hardware mapping onto the **ESP32-P4 Function EV Board**.

---

## 1. Generic Zephyr RTOS Network Stack Architecture

```text
+-----------------------------------------------------------------------------------+
|                           1. APPLICATION LAYER (Layer 7)                          |
|                                                                                   |
|  +-------------------------------------+   +-----------------------------------+  |
|  |       DHCPv4 Client Engine          |   |  BSD Sockets API / Network Shell  |  |
|  |  (zephyr/subsys/net/lib/dhcpv4/)    |   |  (zephyr/subsys/net/lib/sockets/) |  |
|  +-------------------------------------+   +-----------------------------------+  |
+------------------------------------------+----------------------------------------+
                                           |
                                           v
+-----------------------------------------------------------------------------------+
|                     2. NETWORK MANAGEMENT & BUFFER POOLS                          |
|                                                                                   |
|  +-------------------------------------+   +-----------------------------------+  |
|  |  Network Management Core (net_mgmt) |   |  Packet & Buffer Pools (net_pkt)  |  |
|  |    (zephyr/subsys/net/ip/net_mgmt)  |   |    (zephyr/subsys/net/ip/net_pkt) |  |
|  +-------------------------------------+   +-----------------------------------+  |
+------------------------------------------+----------------------------------------+
                                           |
                                           v
+-----------------------------------------------------------------------------------+
|                         3. TRANSPORT LAYER (Layer 4)                              |
|                                                                                   |
|  +-------------------------------------+   +-----------------------------------+  |
|  |          UDP Protocol               |   |          TCP Protocol             |  |
|  |    (zephyr/subsys/net/ip/udp.c)     |   |    (zephyr/subsys/net/ip/tcp.c)   |  |
|  +-------------------------------------+   +-----------------------------------+  |
+------------------------------------------+----------------------------------------+
                                           |
                                           v
+-----------------------------------------------------------------------------------+
|                          4. NETWORK LAYER (Layer 3)                               |
|                                                                                   |
|  +-------------------------------------+   +-----------------------------------+  |
|  |         IPv4 Core Engine            |   |   ARP & ICMPv4 Protocol Modules   |  |
|  |    (zephyr/subsys/net/ip/ipv4.c)    |   |   (zephyr/subsys/net/ip/arp.c)    |  |
|  +-------------------------------------+   +-----------------------------------+  |
+------------------------------------------+----------------------------------------+
                                           |
                                           v
+-----------------------------------------------------------------------------------+
|                         5. DATA LINK LAYER (Layer 2)                              |
|                                                                                   |
|  +-------------------------------------+   +-----------------------------------+  |
|  |      Zephyr Ethernet L2 Core        |   |   Ethernet Carrier Management     |  |
|  | (zephyr/subsys/net/l2/ethernet/eth) |   | (zephyr/subsys/net/l2/ethernet_mg) |  |
|  +-------------------------------------+   +-----------------------------------+  |
+------------------------------------------+----------------------------------------+
                                           |
                                           v
+-----------------------------------------------------------------------------------+
|                    6. NETWORK DEVICE DRIVER API (HAL Layer)                       |
|                                                                                   |
|           struct eth_driver_api (zephyr/include/zephyr/net/ethernet.h)            |
|                  - send(), get_capabilities(), get_stats()                        |
+-----------------------------------------------------------------------------------+
```

---

## 2. Hardware Mapping on ESP32-P4 Function EV Board

```text
=====================================================================================
                      ZEPHYR DEVICE DRIVERS (Software Layer)
=====================================================================================
  [ESP32 EMAC Driver]             [MII / MDIO Management]      [Pin Mux Conditioning]
  drivers/ethernet/eth_esp32.c    drivers/ethernet/mdio/       eth_esp32_priv.h
  - DMA Descriptor Ring in SRAM   drivers/ethernet/phy_mii.c   - Tie RX_ER/COL to 0
           |                                |                           |
===========|================================|===========================|============
           v                                v                           v
+-----------------------------------------------------------------------------------+
|                        ESP32-P4 SoC (Silicon Revision v1.0)                       |
|                                                                                   |
|  +-----------------------------------------------------------------------------+  |
|  |             Synopsys DesignWare EMAC Controller (Base: 0x50098000)          |  |
|  |             - 10/100 Mbps MAC Engine & Dedicated Master DMA                 |  |
|  +-----------------------------------------------------------------------------+  |
|         |                                      |                                  |
|         v                                      v                                  |
|  +-------------------------------+   +-----------------------------------------+  |
|  | HP_SYSTEM & LP_AON Clock Ctrl |   |         GPIO Matrix & Pin Routing       |  |
|  | - sys_gmac_ctrl0 (RMII = 0x4) |   | - Connects EMAC peripheral to GPIO Pads |  |
|  +-------------------------------+   +-----------------------------------------+  |
+----------------------------------------------------+------------------------------+
                                                     |
=====================================================|===============================
                  GPIO PINOUT (esp32p4_function_ev_board_hpcore-pinctrl)
=====================================================|===============================
                                                     |
         +-------------------------------------------+-----------------------+
         |                                                                   |
         v [SMI Management Bus]                                              v [RMII 50MHz Data Bus]
   GPIO31 -> SMI_MDC  (Clock Output)                                   GPIO50 <- RMII_CLK  (50MHz In)
   GPIO52 <> SMI_MDIO (Bidirectional Data)                             GPIO49 -> RMII_TX_EN (TX Enable)
                                                                       GPIO34 -> RMII_TXD0  (TX Bit 0)
                                                                       GPIO35 -> RMII_TXD1  (TX Bit 1)
                                                                       GPIO28 <- RMII_CRS_DV (Carrier/DV)
                                                                       GPIO29 <- RMII_RXD0  (RX Bit 0)
                                                                       GPIO30 <- RMII_RXD1  (RX Bit 1)
         |                                                                   |
=========|===================================================================|=======
         +-------------------------------------------+-----------------------+
                                                     |
                                                     v
+-----------------------------------------------------------------------------------+
|                    FUNCTION EV BOARD (External Board Hardware)                    |
|                                                                                   |
|  +-----------------------------------------------------------------------------+  |
|  |               IP101GR Fast Ethernet PHY Transceiver (PHY Address: 1)        |  |
|  |               - Onboard 50 MHz RMII Reference Clock Oscillator              |  |
|  |               - 100BASE-TX Auto-Negotiation & MLT-3 Encoding/Decoding       |  |
|  +-----------------------------------------------------------------------------+  |
|                                        |                                          |
|                                        v                                          |
|  +-----------------------------------------------------------------------------+  |
|  |                Pulse Transformer & Magnetics Isolation Module               |  |
|  +-----------------------------------------------------------------------------+  |
|                                        |                                          |
|                                        v                                          |
|  +-----------------------------------------------------------------------------+  |
|  |           8P8C (RJ-45) Ethernet Jack (Cat5e Twisted Pair to Router)         |  |
|  +-----------------------------------------------------------------------------+  |
+-----------------------------------------------------------------------------------+
```

---

## 3. End-to-End Packet Data Path Flow

```text
[TRANSMIT / OUTBOUND FLOW (TX)]:
  1. DHCP Client (App)     : Prepares RFC 2131 payload (240 bytes)
  2. UDP Stack (L4)        : Prepends UDP header (Port 68 -> 67, +8 bytes)
  3. IPv4 Stack (L3)       : Prepends IPv4 header (IP 0.0.0.0 -> 255.255.255.255, +20 bytes)
  4. Ethernet L2 (L2)      : Prepends MAC header (Client MAC -> Broadcast, +14 bytes)
  5. eth_esp32 Driver      : Copies frame into DMA TX Descriptor Ring in SRAM
  6. DWC EMAC Engine       : DMA reads buffer and streams 2-bit symbols over RMII at 50 MHz
  7. IP101GR PHY (L1)      : Encodes RMII symbols into differential MLT-3 voltage pulses
  8. RJ-45 Connector       : Transmits pulses over copper twisted-pair cable to Router

─────────────────────────────────────────────────────────────────────────────────────

[RECEIVE / INBOUND FLOW (RX)]:
  1. RJ-45 Connector       : Receives differential MLT-3 electrical pulses from Router
  2. IP101GR PHY (L1)      : Demodulates electrical pulses into 50 MHz RMII digital stream
  3. DWC EMAC Engine       : DMA pushes 342-byte frame into RX Descriptor Ring in SRAM
  4. eth_esp32 ISR / Thread: Verifies CRC FCS, wraps raw buffer into net_pkt structure
  5. Ethernet L2 (L2)      : Strips 14-byte MAC header and validates EtherType 0x0800 (IPv4)
  6. IPv4 Stack (L3)       : Strips 20-byte IPv4 header and validates Protocol 17 (UDP)
  7. UDP Stack (L4)        : Strips 8-byte UDP header and routes to Port 68 (bootpc)
  8. DHCP Client (App)     : Parses offered IP lease (192.168.1.130), Gateway & DNS
```

---

## 4. Key Configuration Files

- [`prj.conf`](prj.conf): Network stack buffer allocations, DHCP client options, and stack sizes.
- [`app.overlay`](app.overlay): Device tree overlay enabling `&eth` and configuring the chosen UART console.
- [`src/main.c`](src/main.c): Application entry point, event listeners (`net_mgmt`), and real-time OSI packet dissection.
