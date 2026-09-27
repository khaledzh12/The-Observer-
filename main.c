#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>

#include "M.h"

int main(void) {
    network_t *net = calloc(1, sizeof(network_t));
    if (!net) {
        perror("Allocation error");
        return 1;
    }

    network_info_t net_info;

    if (get_network_info(&net_info) != 0) {
        fprintf(stderr, "Failed to read network information.\n");
        free(net);
        return 1;
    }

    discover_devices(net, &net_info);

    char input[64];

    while (1) {
        display_devices(net);
        printf("\nEnter Device ID to inspect (or -1 to exit): ");

        if (!fgets(input, sizeof(input), stdin)) {
            printf("\nExiting Network Monitor.\n");
            break;
        }

        errno = 0;
        char *end = NULL;
        long choice = strtol(input, &end, 10);

        if (errno != 0 || end == input) {
            printf("Invalid Device ID! Try again.\n\n");
            continue;
        }

        while (*end == ' ' || *end == '\t') end++;
        if (*end != '\n' && *end != '\0') {
            printf("Invalid Device ID! Try again.\n\n");
            continue;
        }

        if (choice == -1) {
            printf("Exiting Network Monitor.\n");
            break;
        }

        if (choice >= 0 && choice < net->device_count && choice <= INT_MAX) {
            inspect_device(&net->devices[(int)choice]);
        } else {
            printf("Invalid Device ID! Try again.\n\n");
        }
    }

    free(net);
    return 0;
}
