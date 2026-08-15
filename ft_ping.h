#pragma once

#include <stdint.h>
#include <stddef.h>
#include <netinet/in.h>   /* struct sockaddr_in */

/* 56 data bytes + 8 header bytes = the 64 that real ping reports */
#define PING_PAYLOAD_SIZE 56

typedef struct s_icmp_packet
{
    uint8_t   type;                        // 0
    uint8_t   code;                        // 1
    uint16_t  checksum;                    // 2-3
    uint16_t  id;                          // 4-5
    uint16_t  sequence;                    // 6-7
    uint8_t   payload[PING_PAYLOAD_SIZE];  // 8-63
}   t_icmp_packet;                         // 64 bytes

/* No padding is allowed anywhere in this struct: it is copied byte-for-byte
   onto the wire. If a future edit reorders members or widens a field, the
   compiler must reject it rather than silently sending garbage. */
_Static_assert(sizeof(t_icmp_packet) == 8 + PING_PAYLOAD_SIZE,
    "t_icmp_packet has padding or the wrong size - it must be exactly 64 bytes");


uint16_t ft_checksum(const void *buf, size_t len);
int      resolve_host(const char *host, struct sockaddr_in *out);
