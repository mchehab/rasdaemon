// SPDX-License-Identifier: GPL-2.0

/*
 * The code below came from Andi Kleen/Intel/SUSE mcelog code,
 * released under GNU Public General License, v.2
 */

#include <stdio.h>
#include <string.h>

#include "bitfield.h"
#include "ras-mce-handler.h"

/* Follows Intel IA32 SDM 3b Appendix E.2.1 ++ */

static struct field dunnington_bus_status[] = {
	SBITFIELD(16, "Parity error detected during FSB request phase"),
	FIELD_NULL(17),
	SBITFIELD(20, "Hard Failure response received for a local transaction"),
	SBITFIELD(21, "Parity error on FSB response field detected"),
	SBITFIELD(22, "Parity data error on inbound data detected"),
	FIELD_NULL(23),
	FIELD_NULL(25),
	FIELD_NULL(28),
	FIELD_NULL(31),
	{}
};

static char *dnt_front_error[0xf] = {
	[0x1] = "Inclusion error from core 0",
	[0x2] = "Inclusion error from core 1",
	[0x3] = "Write Exclusive error from core 0",
	[0x4] = "Write Exclusive error from core 1",
	[0x5] = "Inclusion error from FSB",
	[0x6] = "SNP stall error from FSB",
	[0x7] = "Write stall error from FSB",
	[0x8] = "FSB Arbiter Timeout error",
	[0xA] = "Inclusion error from core 2",
	[0xB] = "Write exclusive error from core 2",
};

static char *dnt_int_error[0xf] = {
	[0x2] = "Internal timeout error",
	[0x3] = "Internal timeout error",
	[0x4] = "Intel Cache Safe Technology Queue full error\n"
		"or disabled ways in a set overflow",
	[0x5] = "Quiet cycle timeout error (correctable)",
};

struct field dnt_int_status[] = {
	FIELD(8, dnt_int_error),
	{}
};

struct field dnt_front_status[] = {
	FIELD(0, dnt_front_error),
	{}
};

struct field dnt_cecc[] = {
	SBITFIELD(1, "Correctable ECC event on outgoing core 0 data"),
	SBITFIELD(2, "Correctable ECC event on outgoing core 1 data"),
	SBITFIELD(3, "Correctable ECC event on outgoing core 2 data"),
	{}
};

struct field dnt_uecc[] = {
	SBITFIELD(1, "Uncorrectable ECC event on outgoing core 0 data"),
	SBITFIELD(2, "Uncorrectable ECC event on outgoing core 1 data"),
	SBITFIELD(3, "Uncorrectable ECC event on outgoing core 2 data"),
	{}
};

static void dunnington_decode_bus(struct mce_event *e, uint64_t status)
{
	decode_bitfield(e, status, dunnington_bus_status);
}

static void dunnington_decode_internal(struct mce_event *e, uint64_t status)
{
	uint32_t mca = (status >> 16) & 0xffff;

	if ((mca & 0xfff0) == 0)
		decode_bitfield(e, mca, dnt_front_status);
	else if ((mca & 0xf0ff) == 0)
		decode_bitfield(e, mca, dnt_int_status);
	else if ((mca & 0xfff0) == 0xc000)
		decode_bitfield(e, mca, dnt_cecc);
	else if ((mca & 0xfff0) == 0xe000)
		decode_bitfield(e, mca, dnt_uecc);
}

void dunnington_decode_model(struct mce_event *e)
{
	uint64_t status = e->status;

	if ((status & 0xffff) == 0xe0f)
		dunnington_decode_bus(e, status);
	else if ((status & 0xffff) == (1 << 10))
		dunnington_decode_internal(e, status);
}

