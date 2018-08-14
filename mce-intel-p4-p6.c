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

/* Decode P4 and P6 family (p6old and Core2) model specific errors */

/* [19..24] */
static char *bus_queue_req_type[] = {
	[0] = "BQ_DCU_READ_TYPE",
	[2] = "BQ_IFU_DEMAND_TYPE",
	[3] = "BQ_IFU_DEMAND_NC_TYPE",
	[4] = "BQ_DCU_RFO_TYPE",
	[5] = "BQ_DCU_RFO_LOCK_TYPE",
	[6] = "BQ_DCU_ITOM_TYPE",
	[8] = "BQ_DCU_WB_TYPE",
	[10] = "BC_DCU_WCEVICT_TYPE",
	[11] = "BQ_DCU_WCLINE_TYPE",
	[12] = "BQ_DCU_BTM_TYPE",
	[13] = "BQ_DCU_INTACK_TYPE",
	[14] = "BQ_DCU_INVALL2_TYPE",
	[15] = "BQ_DCU_FLUSHL2_TYPE",
	[16] = "BQ_DCU_PART_RD_TYPE",
	[18] = "BQ_DCU_PART_WR_TYPE",
	[20] = "BQ_DCU_SPEC_CYC_TYPE",
	[24] = "BQ_DCU_IO_RD_TYPE",
	[25] = "BQ_DCU_IO_WR_TYPE",
	[28] = "BQ_DCU_LOCK_RD_TYPE",
	[30] = "BQ_DCU_SPLOCK_RD_TYPE",
	[29] = "BQ_DCU_LOCK_WR_TYPE",
};

/* [25..27] */
static char *bus_queue_error_type[] = {
	[0] = "BQ_ERR_HARD_TYPE",
	[1] = "BQ_ERR_DOUBLE_TYPE",
	[2] = "BQ_ERR_AERR2_TYPE",
	[4] = "BQ_ERR_SINGLE_TYPE",
	[5] = "BQ_ERR_AERR1_TYPE",
};

static struct field p6_shared_status[] = {
	FIELD_NULL(16),
	FIELD(19, bus_queue_req_type),
	FIELD(25, bus_queue_error_type),
	FIELD(25, bus_queue_error_type),
	SBITFIELD(30, "internal BINIT"),
	SBITFIELD(36, "received parity error on response transaction"),
	SBITFIELD(38, "timeout BINIT (ROB timeout)."
		  " No micro-instruction retired for some time"),
	FIELD_NULL(39),
	SBITFIELD(42, "bus transaction received hard error response"),
	SBITFIELD(43, "failure that caused IERR"),
	/* The following are reserved for Core in the SDM. Let's keep them here anyways*/
	SBITFIELD(44, "two failing bus transactions with address parity error (AERR)"),
	SBITFIELD(45, "uncorrectable ECC error"),
	SBITFIELD(46, "correctable ECC error"),
	/* [47..54]: ECC syndrome */
	FIELD_NULL(55),
	{},
};

static struct field p6old_status[] = {
	SBITFIELD(28, "FRC error"),
	SBITFIELD(29, "BERR on this CPU"),
	FIELD_NULL(31),
	FIELD_NULL(32),
	SBITFIELD(35, "BINIT received from external bus"),
	SBITFIELD(37, "Received hard error reponse on split transaction (Bus BINIT)"),
	{}
};

static struct field core2_status[] = {
	SBITFIELD(28, "MCE driven"),
	SBITFIELD(29, "MCE is observed"),
	SBITFIELD(31, "BINIT observed"),
	FIELD_NULL(32),
	SBITFIELD(34, "PIC or FSB data parity error"),
	FIELD_NULL(35),
	SBITFIELD(37, "FSB address parity error detected"),
	{}
};

static struct numfield p6old_status_numbers[] = {
	HEXNUMBER(47, 54, "ECC syndrome"),
	{}
};

static struct {
	int value;
	char *str;
} p4_model []= {
	{16, "FSB address parity"},
	{17, "Response hard fail"},
	{18, "Response parity"},
	{19, "PIC and FSB data parity"},
	{20, "Invalid PIC request(Signature=0xF04H)"},
	{21, "Pad state machine"},
	{22, "Pad strobe glitch"},
	{23, "Pad address glitch"}
};

void p4_decode_model(struct mce_event *e)
{
	uint32_t model = e->status & 0xffff0000L;
	unsigned i;

	for (i = 0; i < ARRAY_SIZE(p4_model); i++) {
		if (model & (1 << p4_model[i].value))
			mce_snprintf(e->error_msg, "%s", p4_model[i].str);
	}
}

void core2_decode_model(struct mce_event *e)
{
	uint64_t status = e->status;

	decode_bitfield(e, status, p6_shared_status);
	decode_bitfield(e, status, core2_status);
	/* Normally reserved, but let's parse anyways: */
	decode_numfield(e, status, p6old_status_numbers);
}

void p6old_decode_model(struct mce_event *e)
{
	uint64_t status = e->status;

	decode_bitfield(e, status, p6_shared_status);
	decode_bitfield(e, status, p6old_status);
	decode_numfield(e, status, p6old_status_numbers);
}
