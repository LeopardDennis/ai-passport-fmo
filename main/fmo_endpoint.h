#pragma once
#include <stdbool.h>
#include <stdint.h>

#define FMO_HOST_MAX 120

typedef struct {
    char host[FMO_HOST_MAX + 1];
    uint16_t port;
} fmo_endpoint_t;

/* Hostname or IPv4 only: no scheme, credentials, port, path or IPv6. */
bool fmo_endpoint_valid(const fmo_endpoint_t *endpoint);
