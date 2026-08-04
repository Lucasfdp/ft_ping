# Stage 3 — The Internet Checksum

**Prerequisite:** [02-icmp-protocol.md](02-icmp-protocol.md) · **Next:** [04-building-sending.md](04-building-sending.md)

---

**Plain version:** a checksum is a small number computed from the rest of a message so the
receiver can detect corruption in transit.

**The algorithm:** sum all 16-bit words of the ICMP message (treat the buffer as an array of
`uint16_t`), fold any carry-out beyond 16 bits back into the low bits ("end-around carry"), then
take the one's complement (bitwise NOT) of the result.

**Ordering matters:** zero the checksum field itself before computing — it can't include its own
value.

**Edge case:** an odd number of bytes. Pad the final leftover byte with a zero byte before summing
it as a 16-bit word.

---

## Check questions

<details><summary>Q1: Why zero the checksum field before calculating it?</summary>

It's part of the same buffer you're summing over. If it already held a stale value, you'd be
computing a checksum that includes an arbitrary previous checksum — corrupting the value you're
trying to produce.
</details>

<details><summary>Q2: What do you do with a leftover odd byte at the end of the buffer?</summary>

Treat it as the high byte of a final 16-bit word with the low byte set to zero, then fold that
word into the running sum like any other.
</details>

---

## Exercise

This is the one function you can fully unit-test offline. Do that.

1. Write `uint16_t checksum(const void *buf, size_t len)`.
2. **Test the self-verifying property:** compute the checksum over a buffer, write it into the
   checksum field, then run `checksum()` over the *whole* buffer again. The result must be `0`.
   That's the receiver-side validation trick and it proves your fold is correct.
3. **Test the odd-length path** explicitly with a 7-byte and a 9-byte buffer. Confirm you're not
   reading one byte past the end (run it under `valgrind` or `-fsanitize=address` — an off-by-one
   here is a classic).
4. **Test against ground truth:** take a real ICMP packet from your Stage 2 tcpdump capture, zero
   its checksum field, run yours over it, and confirm you reproduce the captured value exactly.
5. Test the all-zeros buffer and the all-`0xff` buffer.

**Done when:** all five cases pass and ASan/valgrind is clean. Keep this test harness — you'll want
it again if replies ever look wrong.

> **Note on aliasing:** casting a `char *` buffer to `uint16_t *` can trip strict-aliasing and
> unaligned-access rules. Reading two bytes and combining them, or using `memcpy` into a
> `uint16_t`, is the portable route. Worth knowing even if the naive cast happens to work on x86.

---

## Reading

- **RFC 1071** — "Computing the Internet Checksum". Sections 1 and 4.1 give the algorithm and a
  reference C implementation. This is the definitive source
- **RFC 792** — the Checksum field paragraph in the Echo message spec (states what range it covers)
- `man 1 valgrind` or GCC's `-fsanitize=address` docs — for step 3
