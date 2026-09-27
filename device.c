#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "M.h"

static device_type_t guess_type_from_hostname(const char *hostname);
static const char *device_type_string(device_type_t type);

static const char *device_type_string(device_type_t type) {
    switch (type) {
        case DEVICE_ROUTER:   return "ROUTER";
        case DEVICE_EXTENDER: return "EXTENDER";
        case DEVICE_NETWORK:  return "NETWORK";
        case DEVICE_SERVER:   return "SERVER";
        default:              return "DEVICE";
    }
}

static device_type_t guess_type_from_hostname(const char *hostname) {
    if (!hostname || strlen(hostname) == 0) return DEVICE_GENERIC;

    char h[SIZE_NAME];
    size_t i;

    for (i = 0; hostname[i] && i < sizeof(h) - 1; i++) {
        h[i] = (char)tolower((unsigned char)hostname[i]);
    }
    h[i] = '\0';

    if (strstr(h, "extender") || strstr(h, "repeater") ||
        strstr(h, "range-ext") || strstr(h, "rangeext") ||
        strstr(h, "mesh")) {
        return DEVICE_EXTENDER;
    }

    if (strstr(h, "router") || strstr(h, "gateway")) {
        return DEVICE_ROUTER;
    }

    if (strstr(h, "server") || strstr(h, "nas") ||
        strstr(h, "srv-") || strstr(h, "-srv")) {
        return DEVICE_SERVER;
    }

    if (strstr(h, "switch") || strstr(h, "access-point") ||
        strstr(h, "accesspoint") || strstr(h, "wireless-ap") ||
        strstr(h, "network")) {
        return DEVICE_NETWORK;
    }

    return DEVICE_GENERIC;
}

int identify_device(device_t *device, const network_info_t *net_info_in) {
    if (!device) return -1;

    network_info_t local_info;
    const network_info_t *net_info = net_info_in;

    if (!net_info) {
        if (get_network_info(&local_info) == 0) {
            net_info = &local_info;
        }
    }

    device->type = DEVICE_GENERIC;

    if (net_info && strlen(net_info->gateway) > 0 &&
        strcmp(device->ip, net_info->gateway) == 0) {
        device->type = DEVICE_ROUTER;
        return 0;
    }

    if (net_info && strcmp(device->ip, net_info->ip) == 0) {
        device->type = DEVICE_GENERIC;
        return 0;
    }

    device_type_t hostname_type = guess_type_from_hostname(device->hostname);
    if (hostname_type != DEVICE_GENERIC) {
        device->type = hostname_type;
    }

    return 0;
}

void display_devices(const network_t *net) {
    if (!net) return;

    printf("%-4s %-11s %-15s %-17s %-8s\n",
           "ID", "TYPE", "IP", "MAC", "STATUS");
    printf("=================================================================\n");

    for (int i = 0; i < net->device_count; i++) {
        const device_t *d = &net->devices[i];

        printf(" %-3d %-11s %-15s %-17s %-8s\n",
               i,
               device_type_string(d->type),
               d->ip,
               d->mac,
               d->status == STATUS_ONLINE ? "ONLINE" : "OFFLINE");
    }
}

void display_device_details(const device_t *device) {
    if (!device) return;

    printf("===============================================================\n");
    printf("                            DEVICE                             \n");
    printf("===============================================================\n");
    printf(" IP Address     : %s\n", device->ip);
    printf(" Hostname       : %s\n", strlen(device->hostname) > 0 ? device->hostname : "(unknown)");
    printf(" MAC Address    : %s\n", device->mac);
    printf(" Type           : %s\n", device_type_string(device->type));
    printf(" Status         : %s\n", device->status == STATUS_ONLINE ? "ONLINE" : "OFFLINE");
    printf(" Latency (RTT)  : %.2f ms\n", device->latency);
    printf(" Packet Loss    : %.1f %%\n", device->packet_loss);
    printf(" TTL            : %d\n", device->ttl);
    printf("---------------------------------------------------------------\n");
    printf(" MONITORING TRAFFIC STATS (LIVE)\n");
    printf("   - RX Packets : %-10lu | TX Packets : %-10lu\n", device->rx_packets, device->tx_packets);
    printf("   - RX Bytes   : %-10lu | TX Bytes   : %-10lu\n", device->rx_bytes, device->tx_bytes);
    printf("   - RX Errors  : %-10lu | TX Errors  : %-10lu\n", device->rx_errors, device->tx_errors);
    printf("---------------------------------------------------------------\n");
    printf(" OPEN PORTS & PROTOCOLS:\n");

    if (device->port_count == 0) {
        printf("   [No common open TCP ports detected]\n");
    } else {
        for (int i = 0; i < device->port_count; i++) {
            printf("   - Port %d/TCP : OPEN\n", device->ports[i].port_number);
        }
    }

    printf("   - Protocol Flags: ");
    if (device->protocols_seen & PROTOCOL_TCP) printf("[TCP] ");
    if (device->protocols_seen & PROTOCOL_HTTP) printf("[HTTP] ");
    if (device->protocols_seen & PROTOCOL_HTTPS) printf("[HTTPS] ");
    if (device->protocols_seen & PROTOCOL_SSH) printf("[SSH] ");
    if (device->protocols_seen & PROTOCOL_DNS) printf("[DNS] ");
    if (device->protocols_seen == PROTOCOL_NONE) printf("[NONE]");
    printf("\n");
    printf("---------------------------------------------------------------\n");
    printf(" Press [q] to return to the device list\n");
    printf("===============================================================\n");
}


