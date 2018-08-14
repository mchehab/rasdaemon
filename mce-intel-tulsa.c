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

#include <string.h>
#include <stdio.h>

#include "ras-mce-handler.h"
#include "bitfield.h"

/* See IA32 SDM Vol3B Appendix E.4.1 ff */

static struct numfield corr_numbers[] = {
	NUMBER(32, 39, "Corrected events"),
	{}
};

static struct numfield ecc_numbers[] = {
	HEXNUMBER(44, 51, "ECC syndrome"),
	{},
};

static struct field tls_bus_status[] = {
	SBITFIELD(16, "Parity error detected during FSB request phase"),
	SBITFIELD(17, "Partity error detected on Core 0 request's address field"),
	SBITFIELD(18, "Partity error detected on Core 1 request's address field"),
	FIELD_NULL(19),
	SBITFIELD(20, "Parity error on FSB response field detected"),
	SBITFIELD(21, "FSB data parity error on inbound date detected"),
	SBITFIELD(22, "Data parity error on data received from Core 0 detected"),
	SBITFIELD(23, "Data parity error on data received from Core 1 detected"),
	SBITFIELD(24, "Detected an Enhanced Defer parity error phase A or phase B"),
	SBITFIELD(25, "Data ECC event to error on inbound data correctable or uncorrectable"),
	SBITFIELD(26, "Pad logic detected a data strobe glitch or sequencing error"),
	SBITFIELD(27, "Pad logic detected a request strobe glitch or sequencing error"),
	FIELD_NULL(28),
	FIELD_NULL(31),
	{}
};

static char *tls_front_error[0xf] = {
	[0x1] = "Inclusion error from core 0",
	[0x2] = "Inclusion error from core 1",
	[0x3] = "Write Exclusive error from core 0",
	[0x4] = "Write Exclusive error from core 1",
	[0x5] = "Inclusion error from FSB",
	[0x6] = "SNP stall error from FSB",
	[0x7] = "Write stall error from FSB",
	[0x8] = "FSB Arbiter Timeout error",
	[0x9] = "CBC OOD Queue Underflow/overflow",
};

static char *tls_int_error[0xf] = {
	[0x1] = "Enhanced Intel SpeedStep Technology TM1-TM2 Error",
	[0x2] = "Internal timeout error",
	[0x3] = "Internal timeout error",
	[0x4] = "Intel Cache Safe Technology Queue full error\n"
		"or disabled ways in a set overflow",
};

struct field tls_int_status[] = {
	FIELD(8, tls_int_error),
	{}
};

struct field tls_front_status[] = {
	FIELD(0, tls_front_error),
	{}
};

struct field tls_cecc[] = {
	SBITFIELD(0, "Correctable ECC event on outgoing FSB data"),
	SBITFIELD(1, "Correctable ECC event on outgoing core 0 data"),
	SBITFIELD(2, "Correctable ECC event on outgoing core 1 data"),
	{}
};

struct field tls_uecc[] = {
	SBITFIELD(0, "Uncorrectable ECC event on outgoing FSB data"),
	SBITFIELD(1, "Uncorrectable ECC event on outgoing core 0 data"),
	SBITFIELD(2, "Uncorrectable ECC event on outgoing core 1 data"),
	{}
};

static void tulsa_decode_bus(struct mce_event *e, uint64_t status)
{
	decode_bitfield(e, status, tls_bus_status);
}

static void tulsa_decode_internal(struct mce_event *e, uint64_t status)
{
	uint32_t mca = (status >> 16) & 0xffff;
	if ((mca & 0xfff0) == 0)
		decode_bitfield(e, mca, tls_front_status);
	else if ((mca & 0xf0ff) == 0)
		decode_bitfield(e, mca, tls_int_status);
	else if ((mca & 0xfff0) == 0xc000)
		decode_bitfield(e, mca, tls_cecc);
	else if ((mca & 0xfff0) == 0xe000)
		decode_bitfield(e, mca, tls_uecc);
}

void tulsa_decode_model(struct mce_event *e)
{
	decode_numfield(e, e->status, corr_numbers);
	if (e->status & (1ULL << 52))
		decode_numfield(e, e->status, ecc_numbers);
	/* MISC register not documented in the SDM. Let's just dump hex for now. */
	if (e->status &  MCI_STATUS_MISCV)
		mce_snprintf(e->mcistatus_msg, "MISC format %llx value %llx\n",
			     (long long)(e->status >> 40) & 3,
			     (long long)e->misc);

	if ((e->status & 0xffff) == 0xe0f)
		tulsa_decode_bus(e, e->status);
	else if ((e->status & 0xffff) == (1 << 10))
		tulsa_decode_internal(e, e->status);
}
