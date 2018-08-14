/*
 * The code below came from Andi Kleen/Intel/SuSe mcelog code,
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

#include <stdint.h>

/* Generic bitfield decoder */

struct field {
	unsigned start_bit;
	char **str;
	unsigned stringlen;
};

struct numfield {
	unsigned start, end;
	char *name;
	char *fmt;
	int force;
};

#define FIELD(start_bit, name) { start_bit, name, ARRAY_SIZE(name) }
#define FIELD_NULL(start_bit) { start_bit, NULL, 0 }
#define SBITFIELD(start_bit, string) { start_bit, ((char * [2]) { NULL, string }), 2 }

#define NUMBER(start, end, name) { start, end, name, "%Lu", 0 }
#define NUMBERFORCE(start, end, name) { start, end, name, "%Lu", 1 }
#define HEXNUMBER(start, end, name) { start, end, name, "%Lx", 0 }
#define HEXNUMBERFORCE(start, end, name) { start, end, name, "%Lx", 1 }

struct mce_event;

void decode_bitfield(struct mce_event *e, uint64_t status,
		     struct field *fields);
void decode_numfield(struct mce_event *e, uint64_t status,
		     struct numfield *fields);

#define MASK(x) ((1ULL << (1 + (x))) - 1)
#define EXTRACT(v, a, b) (((v) >> (a)) & MASK((b)-(a)))

static inline int test_prefix(int nr, uint32_t value)
{
	return ((value >> nr) == 1);
}

/* Ancillary routines */

unsigned bitfield_msg(char *buf, size_t len, const char **bitarray,
		      unsigned array_len,
		      unsigned bit_offset, unsigned ignore_bits,
		      uint64_t status);
