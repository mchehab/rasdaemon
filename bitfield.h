/* SPDX-License-Identifier: GPL-2.0 */
/*
 * The code below came from Andi Kleen/Intel/SUSE mcelog code,
 * released under GNU Public General License, v.2
 */

#include <stdint.h>

/* Generic bitfield decoder */

struct field {
	unsigned int start_bit;
	char **str;
	unsigned int stringlen;
};

struct numfield {
	unsigned int start, end;
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
#define EXTRACT(v, a, b) (((v) >> (a)) & MASK((b) - (a)))

static inline int test_prefix(int nr, uint32_t value)
{
	return ((value >> nr) == 1);
}

/* Ancillary routines */

unsigned int bitfield_msg(char *buf, size_t len, const char * const *bitarray,
			  unsigned int array_len,
			  unsigned int bit_offset, unsigned int ignore_bits,
			  uint64_t status);
