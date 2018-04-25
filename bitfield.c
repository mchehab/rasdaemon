/*
 * Copyright (C) 2013 Mauro Carvalho Chehab <mchehab+redhat@kernel.org>
 *
 * The code below were adapted from Andi Kleen/Intel/SuSe mcelog code,
 * released under GNU Public General License, v.2
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
*/

#include <string.h>
#include <stdio.h>

#include "ras-mce-handler.h"
#include "bitfield.h"

unsigned bitfield_msg(char *buf, size_t len, const char **bitarray,
		      unsigned array_len,
		      unsigned bit_offset, unsigned ignore_bits,
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
		} else
			mce_snprintf(e->error_msg, "%s", s);
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
