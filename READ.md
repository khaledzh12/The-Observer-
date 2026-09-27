# Monitor 

## Overview
**Monitor ** It is a tool used as a command-line utility written in C and designed for Linux systems.

Tool Functionality: The tool scans the network to detect local devices, displays IP and MAC addresses, checks a small 
set of ports, monitors and configures specific devices and TTL values, and tracks sent and lost packets as well as the number of errors.

## Features

- Displays the local network interface and local IPv4 addresses
- detect the defult gateway
- Display IP and MAC Addresses
- Port Scanning
- Display Device Type and Status
- Packet and Latency Monitoring
- RX/TX Monitoring and Statistics Display

## Projict Files

| File | Description |
| --- | --- |
| `main.c` | The main file for running the tool and handling user input |
| `network` | It contains information about the IP address, MAC address, and local network |
| `discovery.c` | It performs device discovery and ping tests, checks ports, and provides real-time monitoring |
| `device.c` | Identifies the device type and displays device information |
| `M.h` |  inside it Struct and enums and Constants and Prototypes |

## How the Program Works

The program retrieves local network information 
such as IP and MAC addresses, and then performs a ping sweep on addresses 1 through 254.

```text
/proc/net/arp
```
To retrieve the IP and MAC addresses of the detected devices

The tool attempts to determinethe TTL, open TCP ports, and basic device type.

## The PORTS we focus on

| Port | Service |
| --- | --- |
| 22 | SSH |
| 53 | DNS |
| 443 | HTTPS |
| 80 | HTTP |
| 139 | NetBIOS |
| 445 | SMB |
| 631 | IPP |
| 8080 | HTTP Alternative|
| 9100 | Printr Service |

## Requirements for Operation
 
 - Linux
 - gcc 
 - IPv4 network
 - `ping`
 - Access to  ` /proc/net/arp`, `/proc/net/route`, and 
 `/proc/net/dev`

 ## Build 

 ```bash
 gcc -Wall -Wextra -Wpedantic -std=c11 main.c network.c discovery.c device.c -o MMM
 ```

 # Run the Tool
 ```bash
 ./MMM
 ```
 Example Output

```text
ID   TYPE        IP              MAC               STATUS  
=================================================================
 0   ROUTER      192.168.100.1   3C:FF:D8:17:0C:4A ONLINE  
 1   DEVICE      192.168.100.20  16:00:A7:0A:13:76 ONLINE  
 2   DEVICE      192.168.100.7   32:65:82:1A:71:22 ONLINE  
 3   DEVICE      192.168.100.100 C0:C9:E3:1E:F6:0E ONLINE  
 4   DEVICE      192.168.100.8   5C:C1:D7:EC:60:AC ONLINE  
 5   DEVICE      192.168.100.6   E2:A8:E6:3D:CC:D8 ONLINE  
 6   DEVICE      192.168.100.241 98:DF:82:D2:DD:E0 ONLINE  
 7   DEVICE      192.168.100.4   B0:95:75:1E:17:63 ONLINE 
 ```

 Enter the ID of the selected device to view its details

 Press q to return to the device list 

 Press -1 for Exite
 
