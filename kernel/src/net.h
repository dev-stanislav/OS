#ifndef NET_H
#define NET_H

#include <stdint.h>

void net_init(void);
void net_poll(void);
void net_ping_gateway(void);
uint8_t net_ready(void);
uint8_t net_gateway_known(void);
uint8_t net_ping_ok(void);

#endif
