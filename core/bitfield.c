// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2013 Mauro Carvalho Chehab <mchehab+huawei@kernel.org>
 *
 * The code below were adapted from Andi Kleen/Intel/SUSE mcelog code,
 * released under GNU Public General License, v.2.
 */

#include <stdio.h>
#include <string.h>

#include "core/bitfield.h"
#include "events-arch-x86/ras-mce-handler.h"

/**
 * bitfield_msg - format the names of set bits
 * @buf: destination buffer
 * @len: size of @buf
 * @bitarray: array mapping bit positions to names
 * @array_len: number of entries in @bitarray
 * @bit_offset: position of the first described bit in @status
 * @ignore_bits: mask which suppresses output when any masked bit is set
 * @status: value to decode
 *
 * Unknown set bits are formatted as ``BITn``. Output is truncated before a
 * name which does not fit, and @buf is always terminated when @len is nonzero.
 *
 * Return:
 * number of bytes written, excluding the terminating null byte.
 */
unsigned int bitfield_msg(char *buf, size_t len, const char * const *bitarray,
			  unsigned int array_len,
			  unsigned int bit_offset, unsigned int ignore_bits,
			  uint64_t status)
{
	unsigned int i;
	char *p = buf;

	if (!buf || !len)
		return 0;
	buf[0] = '\0';

	for (i = 0; i < array_len; i++) {
		const char *name;
		size_t needed;

		if (status & ignore_bits)
			continue;
		if (i + bit_offset < 64 && status & (1ULL << (i + bit_offset))) {
			int n;
			size_t used;
			char bit_name[sizeof("BIT63")];

			if (!bitarray[i]) {
				snprintf(bit_name, sizeof(bit_name), "BIT%d",
					 i + bit_offset);
				name = bit_name;
			} else {
				name = bitarray[i];
			}

			needed = strlen(name) + (p != buf ? 2 : 0);
			if (needed >= len)
				break;

			if (p != buf) {
				n = snprintf(p, len, ", ");
				if (n < 0 || (size_t)n >= len)
					break;
				len -= n;
				p += n;
			}
			n = snprintf(p, len, "%s", name);
			if (n < 0 || (size_t)n >= len)
				break;
			used = n;
			len -= used;
			p += used;
		}
	}

	*p = '\0';
	return p - buf;
}

/**
 * bitmask - produce a mask covering a zero-based maximum value
 * @i: maximum value to represent
 *
 * Return:
 * the smallest all-ones mask greater than or equal to @i.
 */
static uint64_t bitmask(uint64_t i)
{
	uint64_t mask = 1;

	while (mask < i)
		mask = (mask << 1) | 1;
	return mask;
}

/**
 * decode_bitfield - append decoded symbolic fields to an MCE event
 * @e: event receiving decoded messages
 * @status: machine-check status value
 * @fields: null-terminated field description array
 */
void decode_bitfield(struct mce_event *e, uint64_t status,
		     struct field *fields)
{
	struct field *f;

	for (f = fields; f->str; f++) {
		uint64_t v = (status >> f->start_bit) & bitmask(f->stringlen - 1);
		char *s = NULL;

		if (v < f->stringlen)
			s = f->str[v];
		if (!s) {
			if (v == 0)
				continue;
			mce_snprintf(e->error_msg, "<%u:%llx>",
				     f->start_bit, (long long)v);
		} else {
			mce_snprintf(e->error_msg, "%s", s);
		}
	}
}

/**
 * decode_numfield - append decoded numeric fields to an MCE event
 * @e: event receiving decoded messages
 * @status: machine-check status value
 * @fields: null-terminated numeric field description array
 */
void decode_numfield(struct mce_event *e, uint64_t status,
		     struct numfield *fields)
{
	struct numfield *f;

	for (f = fields; f->name; f++) {
		uint64_t mask = (1ULL << (f->end - f->start + 1)) - 1;
		uint64_t v = (status >> f->start) & mask;

		if (v > 0 || f->force) {
			char fmt[32] = {0};

			snprintf(fmt, 32, "%%s: %s\n", f->fmt ? f->fmt : "%Lu");
			mce_snprintf(e->error_msg, fmt, f->name, v);
		}
	}
}
