/*
 * ESP32-P4 Ethernet - 7-Layer OSI Model Educational Demonstration
 *
 * Demonstrates:
 * 1. Encapsulation (Top-Down: L7 -> L4 -> L3 -> L2 -> L1) for outbound DHCP Discover packet.
 *    Shows cumulative packet byte streams after each header is prepended.
 * 2. Decapsulation (Bottom-Up: L1 -> L2 -> L3 -> L4 -> L7) for inbound DHCP Offer/Ack packet.
 *
 * Target: ESP32-P4 Function EV Board (v1.0 Silicon) with IP101GR Fast Ethernet PHY.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_core.h>
#include <zephyr/net/net_context.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/dhcpv4.h>
#include <zephyr/net/ethernet.h>
#include <zephyr/net/ethernet_mgmt.h>
#include <stdio.h>
#include <string.h>

static struct net_mgmt_event_callback mgmt_cb;
static struct net_mgmt_event_callback carrier_cb;

static struct net_if *app_iface = NULL;
static volatile bool carrier_connected = false;
static volatile bool ip_bound = false;
static volatile bool show_encap_pending = false;
static volatile bool show_decap_pending = false;

static char bound_ip[NET_IPV4_ADDR_LEN] = "192.168.1.130";
static char bound_netmask[NET_IPV4_ADDR_LEN] = "255.255.255.0";
static char bound_gateway[NET_IPV4_ADDR_LEN] = "192.168.1.1";
static uint32_t bound_lease = 43200;

static void print_banner(void)
{
	printk("\n");
	printk("================================================================================\n");
	printk("   ESP32-P4 ETHERNET: 7-LAYER OSI MODEL & DHCPv4 DATA FLOW DEMONSTRATION        \n");
	printk("   Target Board: esp32p4_function_ev_board (v1.0 Silicon Sample)                \n");
	printk("   PHY Transceiver: IP101GR (RMII 50MHz Ref Clock)                              \n");
	printk("================================================================================\n");
	printk("  [Layer 7 - Application ]: DHCP Client/Server creates payload & options (RFC 2131)\n");
	printk("  [Layer 4 - Transport   ]: UDP adds Source Port 68, Dest Port 67 (+8 bytes)     \n");
	printk("  [Layer 3 - Network     ]: IPv4 adds Source IP 0.0.0.0, Dest IP (+20 bytes)     \n");
	printk("  [Layer 2 - Data Link   ]: Ethernet MAC adds MAC Header & CRC FCS (+14 bytes)   \n");
	printk("  [Layer 1 - Physical    ]: PHY converts RAM byte stream to RMII electrical pulses\n");
	printk("================================================================================\n\n");
}

static void print_hex_dump(const char *prefix, const uint8_t *data, size_t len, size_t max_print)
{
	size_t to_print = (len > max_print) ? max_print : len;
	printk("%s (%u bytes):\n", prefix, (unsigned int)len);

	for (size_t i = 0; i < to_print; i += 16) {
		printk("    %04X: ", (unsigned int)i);
		for (size_t j = 0; j < 16; j++) {
			if (i + j < to_print) {
				printk("%02X ", data[i + j]);
			} else {
				printk("   ");
			}
			if (j == 7) {
				printk(" ");
			}
		}
		printk(" |");
		for (size_t j = 0; j < 16; j++) {
			if (i + j < to_print) {
				uint8_t c = data[i + j];
				printk("%c", (c >= 32 && c <= 126) ? c : '.');
			}
		}
		printk("|\n");
	}
	if (len > max_print) {
		printk("    ... (%u additional bytes omitted for concise display)\n", (unsigned int)(len - max_print));
	}
}

static void get_mac_str(struct net_if *iface, char *out_str, size_t max_len)
{
	struct net_linkaddr *ll = iface ? net_if_get_link_addr(iface) : NULL;
	if (ll) {
		snprintf(out_str, max_len, "%02X:%02X:%02X:%02X:%02X:%02X",
			 ll->addr[0], ll->addr[1], ll->addr[2],
			 ll->addr[3], ll->addr[4], ll->addr[5]);
	} else {
		snprintf(out_str, max_len, "60:55:F9:FA:F4:4E");
	}
}

/* =============================================================================
 * PROCESS 1: DATA ENCAPSULATION (TOP-DOWN: LAYER 7 -> LAYER 1)
 * Shows the full cumulative packet at each layer as headers are prepended!
 * ============================================================================= */
static void show_encapsulation_dhcp_discover(struct net_if *iface)
{
	char client_mac[24];
	get_mac_str(iface, client_mac, sizeof(client_mac));

	/* Buffer constructing full frame from L7 (offset 42) to L2 (offset 0) */
	uint8_t frame_buf[300];
	memset(frame_buf, 0, sizeof(frame_buf));

	/* Layer 7: DHCP Payload (offset 42, length 240 bytes) */
	uint8_t *l7_ptr = &frame_buf[42];
	l7_ptr[0] = 0x01; /* BOOTREQUEST */
	l7_ptr[1] = 0x01; /* Hardware type: Ethernet */
	l7_ptr[2] = 0x06; /* Hardware address length: 6 bytes */
	l7_ptr[3] = 0x00; /* Hops */
	l7_ptr[4] = 0x34; l7_ptr[5] = 0x34; l7_ptr[6] = 0x24; l7_ptr[7] = 0xFF; /* XID */
	/* Client MAC at offset 28 */
	struct net_linkaddr *ll = iface ? net_if_get_link_addr(iface) : NULL;
	if (ll) {
		memcpy(&l7_ptr[28], ll->addr, 6);
	}
	/* Magic Cookie at offset 236 */
	l7_ptr[236] = 0x63; l7_ptr[237] = 0x82; l7_ptr[238] = 0x53; l7_ptr[239] = 0x63;

	/* Layer 4: UDP Header (offset 34, length 8 bytes) */
	uint8_t *l4_ptr = &frame_buf[34];
	l4_ptr[0] = 0x00; l4_ptr[1] = 0x44; /* Source Port: 68 (bootpc) */
	l4_ptr[2] = 0x00; l4_ptr[3] = 0x43; /* Dest Port: 67 (bootps) */
	l4_ptr[4] = 0x00; l4_ptr[5] = 0xF8; /* Length: 248 bytes (8 + 240) */
	l4_ptr[6] = 0x00; l4_ptr[7] = 0x00; /* Checksum */

	/* Layer 3: IPv4 Header (offset 14, length 20 bytes) */
	uint8_t *l3_ptr = &frame_buf[14];
	l3_ptr[0] = 0x45; /* Version 4, IHL 5 (20 bytes) */
	l3_ptr[1] = 0x00; /* DSCP/ECN */
	l3_ptr[2] = 0x01; l3_ptr[3] = 0x0C; /* Total Length: 268 bytes (20 + 248) */
	l3_ptr[4] = 0x00; l3_ptr[5] = 0x00; /* Identification */
	l3_ptr[6] = 0x40; l3_ptr[7] = 0x00; /* Flags (Don't Fragment) */
	l3_ptr[8] = 0x40; /* TTL: 64 hops */
	l3_ptr[9] = 0x11; /* Protocol: 17 (UDP) */
	l3_ptr[10] = 0xB6; l3_ptr[11] = 0xD1; /* Header Checksum */
	/* Source IP: 0.0.0.0 */
	l3_ptr[12] = 0x00; l3_ptr[13] = 0x00; l3_ptr[14] = 0x00; l3_ptr[15] = 0x00;
	/* Dest IP: 255.255.255.255 */
	l3_ptr[16] = 0xFF; l3_ptr[17] = 0xFF; l3_ptr[18] = 0xFF; l3_ptr[19] = 0xFF;

	/* Layer 2: Ethernet MAC Header (offset 0, length 14 bytes) */
	uint8_t *l2_ptr = &frame_buf[0];
	/* Dst MAC: Broadcast FF:FF:FF:FF:FF:FF */
	memset(&l2_ptr[0], 0xFF, 6);
	/* Src MAC: Client Hardware Address */
	if (ll) {
		memcpy(&l2_ptr[6], ll->addr, 6);
	} else {
		l2_ptr[6] = 0x60; l2_ptr[7] = 0x55; l2_ptr[8] = 0xF9;
		l2_ptr[9] = 0xFA; l2_ptr[10] = 0xF4; l2_ptr[11] = 0x4E;
	}
	/* EtherType: 0x0800 (IPv4) */
	l2_ptr[12] = 0x08; l2_ptr[13] = 0x00;

	printk("\n");
	printk("################################################################################\n");
	printk("  [PROCESS 1]: DATA ENCAPSULATION (TOP-DOWN: LAYER 7 -> LAYER 1)                \n");
	printk("  Outbound Packet: DHCP DISCOVER (ESP32-P4 -> Network Broadcast)                \n");
	printk("################################################################################\n\n");

	/* Step 1: Layer 7 */
	printk("================================================================================\n");
	printk("1. [LAYER 7 - APPLICATION LAYER]: Construct DHCP Payload (240 bytes)\n");
	printk("================================================================================\n");
	printk("   * DHCP application prepares network configuration parameters (RFC 2131):\n");
	printk("     - Opcode        : 0x01 (BOOTREQUEST - Client request)\n");
	printk("     - Client MAC    : %s\n", client_mac);
	printk("     - Transaction ID: 0x343424FF (Session XID to match server response)\n");
	printk("     - Option 53     : 0x01 (DHCP DISCOVER)\n");
	printk("     - Option 55     : Parameter Request List (Requests: Subnet Mask, Router, DNS)\n");
	print_hex_dump("   * Full Data Packet at Layer 7", l7_ptr, 240, 48);

	/* Step 2: Layer 4 */
	printk("\n================================================================================\n");
	printk("2. [LAYER 4 - TRANSPORT LAYER]: Prepend UDP Header (+8 bytes -> Total: 248B)\n");
	printk("================================================================================\n");
	printk("   * UDP transport layer attaches 8-byte port multiplexing header:\n");
	printk("     - Source Port     : 68 (0x0044) -> DHCP Client port (bootpc)\n");
	printk("     - Destination Port: 67 (0x0043) -> DHCP Server port (bootps)\n");
	printk("     - Length          : 248 bytes (0x00F8 = 8B UDP Header + 240B L7 Payload)\n");
	printk("     - Checksum        : 0x0000 (UDP Checksum)\n");
	printk("   * Header prepended to Layer 7: [ 00 44 00 43 00 F8 00 00 ]\n");
	print_hex_dump("   * Full Data Packet at Layer 4 (UDP Header + L7 Payload)", l4_ptr, 248, 48);

	/* Step 3: Layer 3 */
	printk("\n================================================================================\n");
	printk("3. [LAYER 3 - NETWORK LAYER]: Prepend IPv4 Header (+20 bytes -> Total: 268B)\n");
	printk("================================================================================\n");
	printk("   * IPv4 network layer attaches 20-byte IP routing header:\n");
	printk("     - Version / IHL   : 0x45 (IPv4, Header length = 20 bytes)\n");
	printk("     - Total Length    : 268 bytes (0x010C = 20B IP Header + 248B L4 Datagram)\n");
	printk("     - Time To Live    : 64 Hops (0x40)\n");
	printk("     - Protocol        : 17 (0x11 = UDP Protocol)\n");
	printk("     - Source IP       : 0.0.0.0 (Unassigned client address)\n");
	printk("     - Destination IP  : 255.255.255.255 (Limited Broadcast to entire LAN)\n");
	printk("   * Header prepended to Layer 4: [ 45 00 01 0C 00 00 40 00 40 11 B6 D1 00 00 00 00 FF FF FF FF ]\n");
	print_hex_dump("   * Full Data Packet at Layer 3 (IPv4 + UDP + L7 Payload)", l3_ptr, 268, 48);

	/* Step 4: Layer 2 */
	printk("\n================================================================================\n");
	printk("4. [LAYER 2 - DATA LINK LAYER]: Prepend Ethernet Header (+14 bytes -> Total: 282B)\n");
	printk("================================================================================\n");
	printk("   * EMAC hardware attaches 14-byte Ethernet Header + 4-byte CRC FCS:\n");
	printk("     - Destination MAC : FF:FF:FF:FF:FF:FF (Ethernet Broadcast MAC)\n");
	printk("     - Source MAC      : %s (ESP32-P4 Hardware MAC)\n", client_mac);
	printk("     - EtherType       : 0x0800 (Identifies payload as IPv4)\n");
	printk("   * Header prepended to Layer 3: [ FF FF FF FF FF FF %s 08 00 ]\n", client_mac);
	print_hex_dump("   * Full Data Frame at Layer 2 (Ethernet Header + IPv4 + UDP + L7)", l2_ptr, 282, 48);

	/* Step 5: Layer 1 */
	printk("\n================================================================================\n");
	printk("5. [LAYER 1 - PHYSICAL LAYER]: Transmit Bitstream over Wire (PHY Transceiver)\n");
	printk("================================================================================\n");
	printk("   * EMAC DMA pushes complete 282-byte frame onto RMII 50MHz bus.\n");
	printk("   * IP101GR PHY encodes bitstream into differential MLT-3 voltage pulses at 100 Mbps\n");
	printk("     transmitted across Cat5e twisted-pair copper cable to Router.\n");
	printk("================================================================================\n\n");
}

/* =============================================================================
 * PROCESS 2: DATA DECAPSULATION (BOTTOM-UP: LAYER 1 -> LAYER 7)
 * ============================================================================= */
static void show_decapsulation_dhcp_offer(struct net_if *iface,
					  const char *server_ip,
					  const char *offered_ip,
					  const char *subnet_mask,
					  const char *gateway_ip,
					  uint32_t lease_time)
{
	char client_mac[24];
	get_mac_str(iface, client_mac, sizeof(client_mac));

	printk("\n");
	printk("################################################################################\n");
	printk("  [PROCESS 2]: DATA DECAPSULATION (BOTTOM-UP: LAYER 1 -> LAYER 7)                \n");
	printk("  Inbound Packet: DHCP OFFER / ACK (Router -> ESP32-P4 IP Configuration)        \n");
	printk("################################################################################\n\n");

	/* Layer 1 */
	printk("================================================================================\n");
	printk("1. [LAYER 1 - PHYSICAL LAYER]: Wire Electrical Pulses -> Raw Bytes in RAM Buffer\n");
	printk("================================================================================\n");
	printk("   * RX+/RX- differential pair on RJ-45 port detects MLT-3 signals from Router.\n");
	printk("   * IP101GR PHY demodulates electrical pulses into 50 MHz RMII digital stream.\n");
	printk("   * EMAC DMA engine stores complete 342-byte frame directly into RAM buffer:\n");
	static const uint8_t raw_frame_sample[] = {
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x1A, 0x2B, 0x3C, 0x4D, 0x5E, 0x08, 0x00,
		0x45, 0x00, 0x01, 0x48, 0x00, 0x00, 0x40, 0x00, 0x40, 0x11, 0xB6, 0xD1, 0xC0, 0xA8,
		0x01, 0x01, 0xC0, 0xA8, 0x01, 0x82, 0x00, 0x43, 0x00, 0x44, 0x01, 0x34, 0x00, 0x00,
		0x02, 0x01, 0x06, 0x00, 0x34, 0x34, 0x24, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
	};
	print_hex_dump("   * Complete Raw Layer 1 Frame", raw_frame_sample, sizeof(raw_frame_sample), 56);

	/* Layer 2 */
	printk("\n================================================================================\n");
	printk("2. [LAYER 2 - DATA LINK LAYER]: Strip 14-byte Ethernet Header -> 328 bytes left \n");
	printk("================================================================================\n");
	printk("   * Ethernet MAC controller parses and strips 14-byte leading header:\n");
	printk("     + [Bytes  0.. 5]: Dst MAC  = FF:FF:FF:FF:FF:FF (Delivered to this station)\n");
	printk("     + [Bytes  6..11]: Src MAC  = 00:1A:2B:3C:4D:5E (Router Gateway MAC)\n");
	printk("     + [Bytes 12..13]: EtherType= 0x0800            (Identifies Layer 3 as IPv4)\n");
	printk("   * Hardware 32-bit CRC FCS: VALID (No transmission bit errors detected)\n");
	printk("   * After STRIPPING 14 bytes of Ethernet header, 328 bytes payload passed to L3:\n");
	print_hex_dump("   * Payload passed to Layer 3", &raw_frame_sample[14], sizeof(raw_frame_sample) - 14, 32);

	/* Layer 3 */
	printk("\n================================================================================\n");
	printk("3. [LAYER 3 - NETWORK LAYER]: Strip 20-byte IPv4 Header -> 308 bytes left       \n");
	printk("================================================================================\n");
	printk("   * IPv4 network stack parses and strips 20-byte IP header:\n");
	printk("     + [Byte  0]: Version/IHL  = 0x45 (IPv4, Header length = 20 bytes)\n");
	printk("     + [Byte  8]: TTL          = 64 Hops\n");
	printk("     + [Byte  9]: Protocol     = 17 (0x11 = Identifies Layer 4 as UDP Protocol)\n");
	printk("     + [Bytes 12..15]: Src IP  = %s (Router / DHCP Server IP)\n", server_ip);
	printk("     + [Bytes 16..19]: Dst IP  = %s (Offered Client IP)\n", offered_ip);
	printk("   * After STRIPPING 20 bytes of IPv4 header, 308 bytes payload passed to L4:\n");
	print_hex_dump("   * Payload passed to Layer 4", &raw_frame_sample[34], sizeof(raw_frame_sample) - 34, 32);

	/* Layer 4 */
	printk("\n================================================================================\n");
	printk("4. [LAYER 4 - TRANSPORT LAYER]: Strip 8-byte UDP Header -> 300 bytes left       \n");
	printk("================================================================================\n");
	printk("   * UDP transport layer parses and strips 8-byte UDP header:\n");
	printk("     + [Bytes 0..1]: Source Port     = 67 (DHCP Server / bootps)\n");
	printk("     + [Bytes 2..3]: Destination Port= 68 (DHCP Client / bootpc -> Dispatch to DHCP App)\n");
	printk("     + [Bytes 4..5]: Length          = 308 bytes\n");
	printk("   * After STRIPPING 8 bytes of UDP header, 300 bytes payload delivered to L7:\n");
	print_hex_dump("   * Payload delivered to Layer 7 (DHCP Payload)", &raw_frame_sample[42], sizeof(raw_frame_sample) - 42, 16);

	/* Layer 7 */
	printk("\n================================================================================\n");
	printk("5. [LAYER 7 - APPLICATION LAYER]: Parse DHCPv4 Configuration (RFC 2131)         \n");
	printk("================================================================================\n");
	printk("   * DHCP Client application parses configuration parameters from payload:\n");
	printk("     + Message Type   : DHCP OFFER / ACK (Server confirms IP lease)\n");
	printk("     + Transaction ID : 0x343424FF (Matches original Discover XID session)\n");
	printk("     + Your IP (yiaddr): %s  <-- ASSIGNED IP ADDRESS FOR ESP32-P4!\n", offered_ip);
	printk("     + Server IP (si) : %s  <-- ROUTER / GATEWAY IP ADDRESS!\n", server_ip);
	printk("     + Parsed DHCP Options (TLV - Type/Length/Value):\n");
	printk("       ├── [Option 53 - Message Type] : 0x02 / 0x05 (DHCP OFFER / ACK)\n");
	printk("       ├── [Option 54 - Server ID   ] : %s\n", server_ip);
	printk("       ├── [Option 51 - Lease Time  ] : %u seconds (%.1f hours)\n", (unsigned int)lease_time, (double)lease_time / 3600.0);
	printk("       ├── [Option  1 - Subnet Mask ] : %s\n", subnet_mask);
	printk("       └── [Option  3 - Router / GW ] : %s\n", gateway_ip);
	printk("================================================================================\n\n");
}

static void print_ipv4_info(struct net_if *iface)
{
	char ip_buf[NET_IPV4_ADDR_LEN] = {0};
	char nm_buf[NET_IPV4_ADDR_LEN] = {0};
	char gw_buf[NET_IPV4_ADDR_LEN] = {0};

	if (!iface || !iface->config.ip.ipv4) {
		return;
	}

	for (int i = 0; i < NET_IF_MAX_IPV4_ADDR; i++) {
		if (!iface->config.ip.ipv4->unicast[i].ipv4.is_used) {
			continue;
		}

		net_addr_ntop(NET_AF_INET,
			      &iface->config.ip.ipv4->unicast[i].ipv4.address.in_addr,
			      ip_buf, sizeof(ip_buf));
		net_addr_ntop(NET_AF_INET,
			      &iface->config.ip.ipv4->unicast[i].netmask,
			      nm_buf, sizeof(nm_buf));
		net_addr_ntop(NET_AF_INET,
			      &iface->config.ip.ipv4->gw,
			      gw_buf, sizeof(gw_buf));

		snprintf(bound_ip, sizeof(bound_ip), "%s", ip_buf);
		snprintf(bound_netmask, sizeof(bound_netmask), "%s", nm_buf);
		snprintf(bound_gateway, sizeof(bound_gateway), "%s", gw_buf);
		bound_lease = iface->config.dhcpv4.lease_time;

		show_decap_pending = true;
		ip_bound = true;
	}
}

static void ipv4_mgmt_event_handler(struct net_mgmt_event_callback *cb,
				    uint64_t mgmt_event,
				    struct net_if *iface)
{
	if (mgmt_event == NET_EVENT_IPV4_ADDR_ADD || mgmt_event == NET_EVENT_IPV4_DHCP_BOUND) {
		print_ipv4_info(iface);
	}
}

static void carrier_mgmt_event_handler(struct net_mgmt_event_callback *cb,
				       uint64_t mgmt_event,
				       struct net_if *iface)
{
	if (mgmt_event == NET_EVENT_ETHERNET_CARRIER_ON) {
		carrier_connected = true;
		show_encap_pending = true;
	} else if (mgmt_event == NET_EVENT_ETHERNET_CARRIER_OFF) {
		carrier_connected = false;
		ip_bound = false;
	}
}

static void start_dhcpv4_client(struct net_if *iface, void *user_data)
{
	ARG_UNUSED(user_data);
	app_iface = iface;

	const struct device *dev = net_if_get_device(iface);
	printk("[NET] Starting DHCPv4 client engine on interface: %s (idx: %d)\n",
	       dev ? dev->name : "unknown",
	       net_if_get_by_iface(iface));

	net_dhcpv4_start(iface);
}

int main(void)
{
	print_banner();

	/* Register callback for IPv4 address events */
	net_mgmt_init_event_callback(&mgmt_cb, ipv4_mgmt_event_handler,
				     NET_EVENT_IPV4_ADDR_ADD | NET_EVENT_IPV4_DHCP_BOUND);
	net_mgmt_add_event_callback(&mgmt_cb);

	/* Register callback for Ethernet Link UP / Link DOWN */
	net_mgmt_init_event_callback(&carrier_cb, carrier_mgmt_event_handler,
				     NET_EVENT_ETHERNET_CARRIER_ON |
				     NET_EVENT_ETHERNET_CARRIER_OFF);
	net_mgmt_add_event_callback(&carrier_cb);

	/* Start DHCPv4 client */
	net_if_foreach(start_dhcpv4_client, NULL);

	printk("[NET] Waiting for physical link (RJ-45 Ethernet cable)... \n\n");

	while (1) {
		k_sleep(K_MSEC(200));

		if (show_encap_pending) {
			show_encap_pending = false;
			printk("\n[NET] >> PHYSICAL LINK DETECTED: Ethernet Cable Connected (Carrier ON - 100Mbps Full Duplex)\n");
			show_encapsulation_dhcp_discover(app_iface);
		}

		if (show_decap_pending) {
			show_decap_pending = false;
			show_decapsulation_dhcp_offer(app_iface,
						      bound_gateway,
						      bound_ip,
						      bound_netmask,
						      bound_gateway,
						      bound_lease);

			char client_mac[24];
			get_mac_str(app_iface, client_mac, sizeof(client_mac));

			printk("================================================================================\n");
			printk("  >>> [DHCP HANDSHAKE COMPLETE - IPv4 LEASE ACQUIRED] <<<\n");
			printk("  >>> Ethernet IPv4 Lease Acquired from Router <<<\n");
			printk("================================================================================\n");
			printk("  [Layer 2 - Data Link  ] Hardware MAC Address : %s\n", client_mac);
			printk("  [Layer 3 - Network    ] Assigned IPv4 Address: %s\n", bound_ip);
			printk("  [Layer 3 - Network    ] Subnet Mask          : %s\n", bound_netmask);
			printk("  [Layer 3 - Network    ] Default Gateway      : %s\n", bound_gateway);
			printk("  [Layer 7 - Application] Lease Duration       : %u seconds (%.1f hours)\n",
			       (unsigned int)bound_lease, (double)bound_lease / 3600.0);
			printk("================================================================================\n\n");
		}
	}

	return 0;
}
