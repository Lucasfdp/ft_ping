#include "ft_ping.h"
#include <sys/socket.h>   /* socket(), AF_INET, SOCK_RAW */
#include <netinet/in.h>   /* IPPROTO_ICMP */
#include <unistd.h>       /* close(), getpid() */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>

uint16_t ft_checksum(const void *buf, size_t len)
{
    const uint8_t *p = buf;
    uint32_t sum = 0;

    while (len > 1) {
        uint16_t word;
        memcpy(&word, p, 2);
        sum += word;
        p += 2;
        len -= 2;
    }
    if (len == 1) {
        sum += ((uint16_t)p[0]) << 8;
    }

    while (sum >> 16)
        sum = (sum & 0xFFFF) + (sum >> 16);

    return (uint16_t)~sum;
}