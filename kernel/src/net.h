#ifndef NET_H
#define NET_H

#include <stdint.h>

void net_init(void);
void net_poll(void);
void net_ping_gateway(void);
uint8_t net_ping_ip(const char *address);
uint8_t net_ready(void);
uint8_t net_gateway_known(void);
uint8_t net_ping_ok(void);

typedef enum {
    NET_FETCH_IDLE,
    NET_FETCH_BUSY,
    NET_FETCH_DONE,
    NET_FETCH_ERROR
} net_fetch_status_t;

uint8_t net_fetch_start(const char *url);
void net_fetch_poll(void);
void net_fetch_cancel(void);
net_fetch_status_t net_fetch_status(void);
const char *net_fetch_body(void);
uint16_t net_fetch_body_length(void);
const char *net_fetch_error(void);

#endif
