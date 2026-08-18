/*
 * ESP32-P4 Ethernet (EMAC / RMII) DHCPv4 Sample Application
 *
 * Demonstrates Ethernet networking on ESP32-P4 Function EV Board with Zephyr RTOS.
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

static struct net_mgmt_event_callback mgmt_cb;
static struct net_mgmt_event_callback carrier_cb;
static bool ip_acquired = false;

static void start_dhcpv4_client(struct net_if *iface, void *user_data)
{
	ARG_UNUSED(user_data);

	const struct device *dev = net_if_get_device(iface);
	printk("[NET] Starting DHCPv4 client on interface: %s (idx: %d)\n",
	       dev ? dev->name : "unknown",
	       net_if_get_by_iface(iface));

	net_dhcpv4_start(iface);
}

static void print_ipv4_info(struct net_if *iface)
{
	char ip_buf[NET_IPV4_ADDR_LEN];
	char nm_buf[NET_IPV4_ADDR_LEN];
	char gw_buf[NET_IPV4_ADDR_LEN];

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

		printk("\n==================================================\n");
		printk("  >>> Ethernet IPv4 Lease Acquired from Router <<<\n");
		printk("  IP Address: %s\n", ip_buf);
		printk("  Netmask:    %s\n", nm_buf);
		printk("  Gateway:    %s\n", gw_buf);
		printk("  Lease Time: %u seconds\n", iface->config.dhcpv4.lease_time);
		printk("==================================================\n\n");
		ip_acquired = true;
	}
}

static void ipv4_mgmt_event_handler(struct net_mgmt_event_callback *cb,
				    uint64_t mgmt_event,
				    struct net_if *iface)
{
	print_ipv4_info(iface);
}

static void carrier_mgmt_event_handler(struct net_mgmt_event_callback *cb,
				       uint64_t mgmt_event,
				       struct net_if *iface)
{
	if (mgmt_event == NET_EVENT_ETHERNET_CARRIER_ON) {
		printk("[NET] >> Ethernet Cable Connected (Carrier ON)\n");
	} else if (mgmt_event == NET_EVENT_ETHERNET_CARRIER_OFF) {
		printk("[NET] >> Ethernet Cable Disconnected (Carrier OFF)\n");
		ip_acquired = false;
	}
}

int main(void)
{
	printk("\n");
	printk("**************************************************\n");
	printk("*   ESP32-P4 Ethernet DHCPv4 Application Demo    *\n");
	printk("*   Board: esp32p4_function_ev_board (v1.0)      *\n");
	printk("**************************************************\n\n");

	/* Register callback for IPv4 address events */
	net_mgmt_init_event_callback(&mgmt_cb, ipv4_mgmt_event_handler,
				     NET_EVENT_IPV4_ADDR_ADD | NET_EVENT_IPV4_DHCP_BOUND);
	net_mgmt_add_event_callback(&mgmt_cb);

	/* Register callback for Ethernet Link UP / Link DOWN */
	net_mgmt_init_event_callback(&carrier_cb, carrier_mgmt_event_handler,
				     NET_EVENT_ETHERNET_CARRIER_ON |
				     NET_EVENT_ETHERNET_CARRIER_OFF);
	net_mgmt_add_event_callback(&carrier_cb);

	/* Start DHCPv4 on all configured network interfaces */
	net_if_foreach(start_dhcpv4_client, NULL);

	printk("[NET] Waiting for Ethernet cable connection and DHCP IP lease...\n");

	while (1) {
		k_sleep(K_SECONDS(2));
		if (ip_acquired) {
			/* Network active - heart beat */
			k_sleep(K_SECONDS(10));
		}
	}

	return 0;
}
