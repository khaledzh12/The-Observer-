#define _DEFAULT_SOURCE

#include <stdio.h>
#include <string.h>
#include <ifaddrs.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <net/if.h>

#if defined(__linux__)
#include <netpacket/packet.h>
#endif

#include "M.h"

static int mask_to_prefix(struct sockaddr *netmask) {
    if (!netmask || netmask->sa_family != AF_INET) return 0;

    uint32_t mask = ntohl(((struct sockaddr_in *)netmask)->sin_addr.s_addr);
    int prefix = 0;

    while (mask) {
        prefix += mask & 1;
        mask >>= 1;
    }

    return prefix;
}

int get_network_info(network_info_t *info) {
    if (!info) return -1;

    memset(info, 0, sizeof(network_info_t));

    struct ifaddrs *ifaddr, *ifa;
    if (getifaddrs(&ifaddr) == -1) return -1;

    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET) continue;
        if (strcmp(ifa->ifa_name, "lo") == 0) continue;

        if (ifa->ifa_flags & IFF_UP) {
            strncpy(info->interface_name, ifa->ifa_name, SIZE_NAME - 1);

            struct sockaddr_in *sa = (struct sockaddr_in *)ifa->ifa_addr;
            inet_ntop(AF_INET, &(sa->sin_addr), info->ip, IP_ADDRESS);

            if (ifa->ifa_netmask) {
                info->prefix_length = mask_to_prefix(ifa->ifa_netmask);
            } else {
                info->prefix_length = 24;
            }

            break;
        }
    }

    freeifaddrs(ifaddr);

    FILE *fp = fopen("/proc/net/route", "r");
    if (fp) {
        char line[256];

        while (fgets(line, sizeof(line), fp)) {
            char iface[64];
            unsigned long dest, gw;

            if (sscanf(line, "%63s %lx %lx", iface, &dest, &gw) == 3) {
                if (dest == 0 && gw != 0) {
                    struct in_addr addr;
                    addr.s_addr = gw;
                    inet_ntop(AF_INET, &addr, info->gateway, IP_ADDRESS);
                    break;
                }
            }
        }

        fclose(fp);
    }

    return 0;
}

int get_interfaces(interface_t interfaces[], int max_interfaces) {
    if (!interfaces || max_interfaces <= 0) return -1;

    struct ifaddrs *ifaddr, *ifa;
    if (getifaddrs(&ifaddr) == -1) return -1;

    int count = 0;
    memset(interfaces, 0, sizeof(interface_t) * max_interfaces);

    for (ifa = ifaddr; ifa != NULL && count < max_interfaces; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr) continue;

        int family = ifa->ifa_addr->sa_family;
        int index = -1;

        for (int i = 0; i < count; i++) {
            if (strcmp(interfaces[i].name, ifa->ifa_name) == 0) {
                index = i;
                break;
            }
        }

        if (index == -1) {
            index = count;
            strncpy(interfaces[index].name, ifa->ifa_name, SIZE_NAME - 1);
            interfaces[index].status = (ifa->ifa_flags & IFF_UP) ? STATUS_ONLINE : STATUS_OFFLINE;
            count++;
        }

        if (family == AF_INET) {
            struct sockaddr_in *sa = (struct sockaddr_in *)ifa->ifa_addr;
            inet_ntop(AF_INET, &(sa->sin_addr), interfaces[index].ip, IP_ADDRESS);
        } else if (family == AF_PACKET) {
            struct sockaddr_ll *s = (struct sockaddr_ll *)ifa->ifa_addr;

            if (s->sll_halen == 6) {
                snprintf(interfaces[index].mac, MAC_ADDRESS,
                         "%02X:%02X:%02X:%02X:%02X:%02X",
                         s->sll_addr[0], s->sll_addr[1], s->sll_addr[2],
                         s->sll_addr[3], s->sll_addr[4], s->sll_addr[5]);
            }
        }
    }

    freeifaddrs(ifaddr);

    for (int i = 0; i < count; i++) {
        get_interface_info(interfaces[i].name, &interfaces[i]);
    }

    return count;
}
 
int get_interface_info(const char *interface_name, interface_t *interface) {
    if (!interface_name || !interface) return -1;

    FILE *fp = fopen("/proc/net/dev", "r");
    if (!fp) return -1;

    char buffer[256];

    if (!fgets(buffer, sizeof(buffer), fp) || !fgets(buffer, sizeof(buffer), fp)) {
        fclose(fp);
        return -1;
    }

    while (fgets(buffer, sizeof(buffer), fp)) {
        char name[64];
        unsigned long rx_b, rx_p, rx_e, tx_b, tx_p, tx_e;

        char *pos = strchr(buffer, ':');
        if (!pos) continue;

        *pos = '\0';
        sscanf(buffer, "%s", name);

        if (strcmp(name, interface_name) == 0) {
            sscanf(pos + 1, "%lu %lu %lu %*u %*u %*u %*u %*u %lu %lu %lu",
                   &rx_b, &rx_p, &rx_e, &tx_b, &tx_p, &tx_e);

            interface->rx_bytes = rx_b;
            interface->rx_packets = rx_p;
            interface->rx_errors = rx_e;
            interface->tx_bytes = tx_b;
            interface->tx_packets = tx_p;
            interface->tx_errors = tx_e;

            fclose(fp);
            return 0;
        }
    }

    fclose(fp);
    return -1;
}
