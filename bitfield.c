// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2013 Mauro Carvalho Chehab <mchehab+huawei@kernel.org>
 *
 * The code below were adapted from Andi Kleen/Intel/SUSE mcelog code,
 * released under GNU Public General License, v.2.
 */

#include <stdio.h>
#include <string.h>

#include "bitfield.h"
#include "ras-mce-handler.h"

unsigned int bitfield_msg(char *buf, size_t len, const char * const *bitarray,
			  unsigned int array_len,
			  unsigned int bit_offset, unsigned int ignore_bits,
			  uint64_t status)
{
	int i, n;
	char *p = buf;

	len--;

	for (i = 0; i < array_len; i++) {
		if (status & ignore_bits)
			continue;
		if (status & (1 <<  (i + bit_offset))) {
			if (p != buf) {
				n = snprintf(p, len, ", ");
				if (n < 0)
					break;
				len -= n;
				p += n;
			}
			if (!bitarray[i])
				n = snprintf(p, len, "BIT%d", i + bit_offset);
			else
				n = snprintf(p, len, "%s", bitarray[i]);
			if (n < 0)
				break;
			len -= n;
			p += n;
		}
	}

	*p = 0;
	return p - buf;
}

static uint64_t bitmask(uint64_t i)
{
	uint64_t mask = 1;

	while (mask < i)
		mask = (mask << 1) | 1;
	return mask;
}

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
