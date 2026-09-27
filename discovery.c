#define _DEFAULT_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <termios.h>
#include <time.h>

#include "M.h"

static int get_device_ttl(const char *ip);
static int ping_device(const char *ip, int *ttl_out, float *rtt_ms_out);
static void get_device_hostname(const char *ip, char *out, size_t out_size);
static int enable_key_mode(struct termios *saved);
static void restore_key_mode(const struct termios *saved, int enabled);
static int wait_for_quit(int seconds);

int discover_devices(network_t *net, const network_info_t *info) {
    if (!net || !info) return -1;

    char base_ip[IP_ADDRESS] = {0};
    strncpy(base_ip, info->ip, sizeof(base_ip) - 1);

    char *last_dot = strrchr(base_ip, '.');
    if (last_dot) *last_dot = '\0';

    for (int i = 1; i < 255; i++) {
        char cmd[128];
        snprintf(cmd, sizeof(cmd), "ping -c 1 -W 1 %s.%d > /dev/null 2>&1 &", base_ip, i);
        system(cmd);
    }

    usleep(600000);

    FILE *fp = fopen("/proc/net/arp", "r");
    if (!fp) return -1;

    char buffer[256];
    if (!fgets(buffer, sizeof(buffer), fp)) {
        fclose(fp);
        return 0;
    }

    char ip[IP_ADDRESS];
    char hw_type[16];
    char flags[16];
    char mac[MAC_ADDRESS];
    char mask[16];
    char dev[SIZE_NAME];
    time_t now = time(NULL);

    while (fgets(buffer, sizeof(buffer), fp)) {
        if (sscanf(buffer, "%15s %15s %15s %17s %15s %63s",
                   ip, hw_type, flags, mac, mask, dev) >= 6) {
            if (strcmp(mac, "00:00:00:00:00:00") != 0 && strcmp(flags, "0x0") != 0) {
                int exists = 0;

                for (int i = 0; i < net->device_count; i++) {
                    if (strcmp(net->devices[i].ip, ip) == 0) {
                        net->devices[i].last_seen = now;
                        net->devices[i].status = STATUS_ONLINE;
                        exists = 1;
                        break;
                    }
                }

                if (!exists && net->device_count < MAX_DEVICES) {
                    device_t *d = &net->devices[net->device_count];
                    memset(d, 0, sizeof(device_t));

                    strncpy(d->ip, ip, IP_ADDRESS - 1);
                    d->ip[IP_ADDRESS - 1] = '\0';

                    for (int k = 0; mac[k] && k < MAC_ADDRESS - 1; k++) {
                        if (mac[k] >= 'a' && mac[k] <= 'f') d->mac[k] = (char)(mac[k] - 32);
                        else d->mac[k] = mac[k];
                    }

                    d->ip_type = IP_TYPE_V4;
                    d->status = STATUS_ONLINE | STATUS_OFFLINE;
                    d->first_seen = now;
                    d->last_seen = now;

                    get_device_hostname(d->ip, d->hostname, sizeof(d->hostname));
                    d->ttl = get_device_ttl(d->ip);
                    scan_device_ports(d);
                    identify_device(d, info);

                    net->device_count++;
                }
            }
        }
    }

    fclose(fp);
    return net->device_count;
}

static int get_device_ttl(const char *ip) {
    int ttl = -1;
    float rtt = 0.0f;
    ping_device(ip, &ttl,&rtt);
    return ttl;
}

static int ping_device(const char *ip, int *ttl_out, float *rtt_ms_out) {
    if (ttl_out) *ttl_out = -1;
    if (rtt_ms_out) *rtt_ms_out = 0.0f;
    if (!ip || strlen(ip) == 0) return -1;

    char cmd[160];
    snprintf(cmd, sizeof(cmd), "ping -c 1 -W 1 %s 2>/dev/null", ip);

    FILE *fp = popen(cmd, "r");
    if (!fp) return -1;

    int got_reply = 0;
    char line[256];

    while (fgets(line, sizeof(line), fp)) {
        char *p = strstr(line, "ttl=");
        if (!p) p = strstr(line, "TTL=");

        if (p) {
            if (ttl_out) *ttl_out = atoi(p + 4);
            got_reply = 1;
        }

        char *t = strstr(line, "time=");
        if (t && rtt_ms_out) {
            *rtt_ms_out = (float)atof(t + 5);
        }
    }

    pclose(fp);
    return got_reply ? 0 : -1;
}

static void get_device_hostname(const char *ip, char *out, size_t out_size) {
    if (!out || out_size == 0) return;

    out[0] = '\0';
    if (!ip) return;

    struct sockaddr_in sa;
    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;

    if (inet_pton(AF_INET, ip, &sa.sin_addr) != 1) return;

    char host[NI_MAXHOST];

    if (getnameinfo((struct sockaddr *)&sa, sizeof(sa),
                    host, sizeof(host), NULL, 0, 0) == 0) {
        strncpy(out, host, out_size - 1);
        out[out_size - 1] = '\0';
    }
}

int scan_device_ports(device_t *device) {
    if (!device) return -1;

    int common_ports[] = {22, 53, 80, 443, 8080, 139, 445, 631, 9100};
    int total = (int)(sizeof(common_ports) / sizeof(common_ports[0]));

    device->port_count = 0;
    device->protocols_seen = PROTOCOL_NONE;

    const unsigned long SYN_FRAME_BYTES = 54;
    const unsigned long RESP_FRAME_BYTES = 60;

    for (int i = 0; i < total; i++) {
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) continue;

        int flags = fcntl(sock, F_GETFL, 0);
        if (flags >= 0) {
            fcntl(sock, F_SETFL, flags | O_NONBLOCK);
        }

        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons((uint16_t)common_ports[i]);

        if (inet_pton(AF_INET, device->ip, &addr.sin_addr) != 1) {
            close(sock);
            continue;
        }

        connect(sock, (struct sockaddr *)&addr, sizeof(addr));

        device->tx_packets++;
        device->tx_bytes += SYN_FRAME_BYTES;

        fd_set fdset;
        FD_ZERO(&fdset);
        FD_SET(sock, &fdset);

        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 150000;

        if (select(sock + 1, NULL, &fdset, NULL, &tv) > 0) {
            int so_error = 0;
            socklen_t len = sizeof(so_error);

            if (getsockopt(sock, SOL_SOCKET, SO_ERROR, &so_error, &len) == 0) {
                device->rx_packets++;
                device->rx_bytes += RESP_FRAME_BYTES;

                if (so_error == 0 && device->port_count < MAX_PORTS) {
                    port_info_t *port = &device->ports[device->port_count];
                    port->port_number = common_ports[i];
                    port->is_open = 1;
                    port->protocol = PROTOCOL_TCP;

                    switch (common_ports[i]) {
                        case 22:
                            port->protocol = PROTOCOL_SSH;
                            device->protocols_seen |= PROTOCOL_SSH;
                            break;
                        case 53:
                            port->protocol = PROTOCOL_DNS;
                            device->protocols_seen |= PROTOCOL_DNS;
                            break;
                        case 80:
                            port->protocol = PROTOCOL_HTTP;
                            device->protocols_seen |= PROTOCOL_HTTP;
                            break;
                        case 443:
                            port->protocol = PROTOCOL_HTTPS;
                            device->protocols_seen |= PROTOCOL_HTTPS;
                            break;
                        default:
                            device->protocols_seen |= PROTOCOL_TCP;
                            break;
                    }

                    device->port_count++;
                }
            }
        } else {
            device->tx_errors++;
        }

        close(sock);
    }

    return device->port_count;
}

static int enable_key_mode(struct termios *saved) {
    if (!saved || !isatty(STDIN_FILENO)) return 0;

    if (tcgetattr(STDIN_FILENO, saved) == -1) return 0;

    struct termios current = *saved;
    current.c_lflag &= (tcflag_t)~(ICANON | ECHO);
    current.c_cc[VMIN] = 0;
    current.c_cc[VTIME] = 0;

    if (tcsetattr(STDIN_FILENO, TCSANOW, &current) == -1) return 0;
    return 1;
}

static void restore_key_mode(const struct termios *saved, int enabled) {
    if (enabled && saved) {
        tcsetattr(STDIN_FILENO, TCSANOW, saved);
    }
}

static int wait_for_quit(int seconds) {
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(STDIN_FILENO, &readfds);

    struct timeval timeout;
    timeout.tv_sec = seconds;
    timeout.tv_usec = 0;

    int ready = select(STDIN_FILENO + 1, &readfds, NULL, NULL, &timeout);
    if (ready <= 0 || !FD_ISSET(STDIN_FILENO, &readfds)) return 0;

    char input[32];
    ssize_t count = read(STDIN_FILENO, input, sizeof(input));
    if (count <= 0) return 0;

    for (ssize_t i = 0; i < count; i++) {
        if (input[i] == 'q' || input[i] == 'Q') return 1;
    }

    return 0;
}

void inspect_device(device_t *device) {
    if (!device) return;

    const unsigned long ICMP_FRAME_BYTES = 98;
    unsigned long ping_sent = 0;
    unsigned long ping_lost = 0;
    unsigned long refresh_count = 0;

    struct termios saved_terminal;
    int key_mode_enabled = enable_key_mode(&saved_terminal);

    while (1) {
        int ttl = -1;
        float rtt = 0.0f;

        ping_sent++;
        device->tx_packets++;
        device->tx_bytes += ICMP_FRAME_BYTES;

        if (ping_device(device->ip, &ttl, &rtt) == 0) {
            device->ttl = ttl;
            device->latency = rtt;
            device->rx_packets++;
            device->rx_bytes += ICMP_FRAME_BYTES;
            device->status = STATUS_ONLINE;
            device->last_seen = time(NULL);
        } else {
            ping_lost++;
            device->rx_errors++;
            device->status = STATUS_OFFLINE;
        }

        device->packet_loss = ping_sent > 0
            ? ((float)ping_lost / (float)ping_sent) * 100.0f
            : 0.0f;

        refresh_count++;
        if (refresh_count % PORT_RESCAN_INTERVAL == 0) {
            scan_device_ports(device);
        }

        identify_device(device, NULL);

        printf("\033[2J\033[H");
        display_device_details(device);
        fflush(stdout);

        if (wait_for_quit(LIVE_REFRESH_SECONDS)) {
            break;
        }
    }

    restore_key_mode(&saved_terminal, key_mode_enabled);
    printf("\033[2J\033[H");
    fflush(stdout);
}
