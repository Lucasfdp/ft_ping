// test_checksum.c - unit tests for ft_checksum()
//
//   make test_checksum && ./test_checksum
//
// No root needed: ft_checksum() opens nothing and depends on nothing.
#include "ft_ping.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    // 1. self-verify
    uint8_t buf[8] = {8,0,0,0, 0x12,0x34, 0,1};
    uint16_t c = ft_checksum(buf, sizeof buf);
    memcpy(buf + 2, &c, 2);              // insert ft_checksum (already network order from your fn — check this)
    assert(ft_checksum(buf, sizeof buf) == 0);

    // 2. odd-length: 7 bytes and 9 bytes — pick known vectors, print and eyeball first
    uint8_t odd7[7] = {0x45,0,0,0x1c,0,0,0};
    printf("7-byte: %04x\n", ft_checksum(odd7, 7));

    // 5. all-zeros / all-0xff
    uint8_t z[8] = {0}, f[8]; memset(f, 0xff, 8);
    printf("zeros: %04x  ff: %04x\n", ft_checksum(z, 8), ft_checksum(f, 8));

    printf("all asserts passed\n");
    return 0;
}