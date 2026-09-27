#ifndef M_H
#define M_H
#include <time.h>

#define SIZE_NAME      64
#define IP_ADDRESS     16
#define MAC_ADDRESS    18

#define MAX_DEVICES    100
#define MAX_INTERFACES 10
#define MAX_PORTS      16

#define LIVE_REFRESH_SECONDS 1
#define PORT_RESCAN_INTERVAL 5

typedef enum {
    PROTOCOL_NONE  = 0,
    PROTOCOL_TCP   = 1 << 0,
    PROTOCOL_UDP   = 1 << 1,
    PROTOCOL_ICMP  = 1 << 2,
    PROTOCOL_HTTP  = 1 << 3,
    PROTOCOL_HTTPS = 1 << 4,
    PROTOCOL_SNMP  = 1 << 5,
    PROTOCOL_MDNS  = 1 << 6,
    PROTOCOL_SMTP  = 1 << 7,
    PROTOCOL_DNS   = 1 << 8,
    PROTOCOL_SSH   = 1 << 9,
    PROTOCOL_FTP   = 1 << 10,
    PROTOCOL_DHCP  = 1 << 11
} protocol_flags_t;

typedef enum {
    DEVICE_GENERIC = 0,
    DEVICE_ROUTER,
    DEVICE_EXTENDER,
    DEVICE_NETWORK,
    DEVICE_SERVER
} device_type_t;

typedef enum {
    STATUS_OFFLINE = 0,
    STATUS_ONLINE
} device_status_t;

typedef enum {
    IP_TYPE_UNKNOWN = 0,
    IP_TYPE_V4,
    IP_TYPE_V6
} ip_type_t;

typedef struct {
    int port_number;
    protocol_flags_t protocol;
    int is_open;
} port_info_t;

typedef struct {
    char name[SIZE_NAME];
    char ip[IP_ADDRESS];
    char mac[MAC_ADDRESS];

    int port_count;
    device_status_t status;

    unsigned long tx_bytes;
    unsigned long rx_bytes;
    unsigned long tx_packets;
    unsigned long rx_packets;
    unsigned long tx_errors;
    unsigned long rx_errors;
} interface_t;

typedef struct {
    char interface_name[SIZE_NAME];
    char ip[IP_ADDRESS];
    char mac[MAC_ADDRESS];
    char gateway[IP_ADDRESS];
    int prefix_length;
} network_info_t;

typedef struct {
    char name[SIZE_NAME];
    char hostname[SIZE_NAME];
    char ip[IP_ADDRESS];
    char mac[MAC_ADDRESS];

    ip_type_t ip_type;
    int ttl;

    device_type_t type;
    device_status_t status;

    protocol_flags_t protocols_seen;

    port_info_t ports[MAX_PORTS];
    int port_count;

    unsigned long rx_bytes;
    unsigned long tx_bytes;
    unsigned long rx_packets;
    unsigned long tx_packets;
    unsigned long rx_errors;
    unsigned long tx_errors;

    float latency;
    float packet_loss;

    time_t first_seen;
    time_t last_seen;

    interface_t interfaces[MAX_INTERFACES];
    int interface_count;
} device_t;

typedef struct {
    device_t devices[MAX_DEVICES];
    int device_count;
} network_t;

int get_network_info(network_info_t *info);
int get_interface_info(const char *interface_name, interface_t *interface);

int discover_devices(network_t *net, const network_info_t *info);
int identify_device(device_t *device, const network_info_t *net_info);
int scan_device_ports(device_t *device);
void inspect_device(device_t *device);

void display_devices(const network_t *net);
void display_device_details(const device_t *device);

#endif
