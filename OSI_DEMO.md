# 7-Layer OSI Model & DHCPv4 Demonstration

This document explains how network data flows through the 7 layers of the OSI model during the **DHCPv4 DORA Handshake** on the ESP32-P4.

---

## 1. Overview of the DORA Process

When an Ethernet cable is plugged in, the device acquires an IPv4 address through 4 steps:

1. **D - DISCOVER**: ESP32-P4 broadcasts a request to locate available DHCP servers.
2. **O - OFFER**: The Router offers an IP address (`192.168.1.130`) and configuration options.
3. **R - REQUEST**: ESP32-P4 formally requests the offered IP address.
4. **A - ACK**: The Router confirms the lease and commits the IP to the client.

---

## 2. Note on Layer 5 (Session) and Layer 6 (Presentation)

In the real-world **TCP/IP Protocol Suite** (RFC standards), **Layers 5, 6, and 7 are unified into the Application Layer (Layer 7)** without separate on-the-wire packet headers:

- **Layer 6 (Presentation Layer Functions)**:
  - Byte encoding, Big-Endian / Network Byte Order conversion (`htonl()` / `ntohl()`), and Option TLV (Type-Length-Value) formatting are handled directly inside the DHCP message payload.
- **Layer 5 (Session Layer Functions)**:
  - Session establishment, matching the Transaction ID (`xid`) across the 4 DORA steps, and lease renewal timers are managed by the DHCP state machine within the application.

> **Summary**: Data flows directly from **Layer 7** into **Layer 4 (UDP)** because Presentation and Session tasks are embedded directly into the DHCP application payload rather than having independent network headers.

---

## 3. What the Console Logs Demonstrate

### Process 1: Data Encapsulation (Top-Down: Layer 7 $\rightarrow$ Layer 1)
*Demonstrated on outbound **DHCP DISCOVER** packet (ESP32-P4 $\rightarrow$ Broadcast):*

- **Layer 7 (Application)**: Prepares the DHCP payload (240 bytes) with XID session ID, client MAC, and Option 55 parameter requests.
- **Layer 4 (Transport)**: Prepends 8-byte UDP header (Src Port `68`, Dst Port `67`) $\rightarrow$ Total: 248 bytes.
- **Layer 3 (Network)**: Prepends 20-byte IPv4 header (Src `0.0.0.0`, Dst `255.255.255.255`) $\rightarrow$ Total: 268 bytes.
- **Layer 2 (Data Link)**: Prepends 14-byte Ethernet MAC header (Src MAC, Dst `FF:FF:FF:FF:FF:FF`) $\rightarrow$ Total: 282 bytes.
- **Layer 1 (Physical)**: EMAC DMA pushes 282 bytes onto RMII 50MHz bus, and the IP101GR PHY converts it to differential MLT-3 voltage pulses over the RJ-45 cable.

### Process 2: Data Decapsulation (Bottom-Up: Layer 1 $\rightarrow$ Layer 7)
*Demonstrated on inbound **DHCP OFFER / ACK** packet (Router $\rightarrow$ ESP32-P4):*

- **Layer 1 (Physical)**: IP101GR PHY detects electrical pulses and DMA transfers raw 342-byte frame into RAM buffer.
- **Layer 2 (Data Link)**: EMAC verifies 32-bit CRC FCS and strips 14-byte Ethernet header $\rightarrow$ 328 bytes passed to L3.
- **Layer 3 (Network)**: IPv4 stack validates IP checksum and strips 20-byte IP header $\rightarrow$ 308 bytes passed to L4.
- **Layer 4 (Transport)**: UDP stack verifies port `68` and strips 8-byte UDP header $\rightarrow$ 300 bytes payload passed to L7.
- **Layer 7 (Application)**: DHCP client parses offered IP (`192.168.1.130`), Gateway (`192.168.1.1`), Subnet Mask (`255.255.255.0`), and Lease Duration (`43200s`).

---

## 4. OSI Layer to Zephyr Source Code Mapping

All paths are relative to the workspace root:

| OSI Layer | Protocol / Function | Zephyr Source Implementation (Relative Path) |
| :--- | :--- | :--- |
| **Layer 7 (Application)** | DHCPv4 Client & Options Parsing | `zephyr/subsys/net/lib/dhcpv4/dhcpv4.c`<br>`esp32p4-ethernet/src/main.c` |
| **Layer 4 (Transport)** | UDP Datagram & Port Handling | `zephyr/subsys/net/ip/udp.c`<br>`zephyr/subsys/net/ip/net_context.c` |
| **Layer 3 (Network)** | IPv4 Packet Routing & ARP | `zephyr/subsys/net/ip/ipv4.c`<br>`zephyr/subsys/net/ip/net_pkt.c` |
| **Layer 2 (Data Link)** | Ethernet MAC Framing & DMA Ring | `zephyr/subsys/net/l2/ethernet/ethernet.c`<br>`zephyr/drivers/ethernet/eth_esp32.c` |
| **Layer 1 (Physical)** | RMII 50MHz Clock & IP101GR PHY | `zephyr/drivers/ethernet/phy_mii.c`<br>`zephyr/drivers/ethernet/eth_esp32_priv.h` |
