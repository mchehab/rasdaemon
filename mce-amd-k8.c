/*
 * Copyright (C) 2013 Mauro Carvalho Chehab <mchehab@redhat.com>
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

#include <stdio.h>
#include <string.h>

#include "ras-mce-handler.h"
#include "bitfield.h"

#define K8_MCE_THRESHOLD_BASE        (MCE_EXTENDED_BANK + 1)      /* MCE_AMD */
#define K8_MCE_THRESHOLD_TOP         (K8_MCE_THRESHOLD_BASE + 6 * 9)

#define K8_MCELOG_THRESHOLD_DRAM_ECC (4 * 9 + 0)
#define K8_MCELOG_THRESHOLD_LINK     (4 * 9 + 1)
#define K8_MCELOG_THRESHOLD_L3_CACHE (4 * 9 + 2)
#define K8_MCELOG_THRESHOLD_FBDIMM   (4 * 9 + 3)

static const char *k8bank[] = {
	"data cache",
	"instruction cache",
	"bus unit",
	"load/store unit",
	"northbridge",
	"fixed-issue reoder"
};

static const char *k8threshold[] = {
	[0 ... K8_MCELOG_THRESHOLD_DRAM_ECC - 1] = "Unknow threshold counter",
	[K8_MCELOG_THRESHOLD_DRAM_ECC] = "MC4_MISC0 DRAM threshold",
	[K8_MCELOG_THRESHOLD_LINK] = "MC4_MISC1 Link threshold",
	[K8_MCELOG_THRESHOLD_L3_CACHE] = "MC4_MISC2 L3 Cache threshold",
	[K8_MCELOG_THRESHOLD_FBDIMM] = "MC4_MISC3 FBDIMM threshold",
	[K8_MCELOG_THRESHOLD_FBDIMM + 1 ...
	 K8_MCE_THRESHOLD_TOP - K8_MCE_THRESHOLD_BASE - 1] =
		"Unknown threshold counter",
};

static const char *transaction[] = {
	"instruction", "data", "generic", "reserved"
};
static const char *cachelevel[] = {
	"0", "1", "2", "generic"
};
static const char *memtrans[] = {
	"generic error", "generic read", "generic write", "data read",
	"data write", "instruction fetch", "prefetch", "evict", "snoop",
	"?", "?", "?", "?", "?", "?", "?"
};
static const char *partproc[] = {
	"local node origin", "local node response",
	"local node observed", "generic participation"
};
static const char *timeout[] = {
	"request didn't time out",
	"request timed out"
};
static const char *memoryio[] = {
	"memory", "res.", "i/o", "generic"
};
static const char *nbextendederr[] = {
	"RAM ECC error",
	"CRC error",
	"Sync error",
	"Master abort",
	"Target abort",
	"GART error",
	"RMW error",
	"Watchdog error",
	"RAM Chipkill ECC error",
	"DEV Error",
	"Link Data Error",
	"Link Protocol Error",
	"NB Array Error",
	"DRAM Parity Error",
	"Link Retry",
	"Tablew Walk Data Error",
	"L3 Cache Data Error",
	"L3 Cache Tag Error",
	"L3 Cache LRU Error"
};
static const char *highbits[32] = {
	[31] = "valid",
	[30] = "error overflow (multiple errors)",
	[29] = "error uncorrected",
	[28] = "error enable",
	[27] = "misc error valid",
	[26] = "error address valid",
	[25] = "processor context corrupt",
	[24] = "res24",
	[23] = "res23",
	/* 22-15 ecc syndrome bits */
	[14] = "corrected ecc error",
	[13] = "uncorrected ecc error",
	[12] = "res12",
	[11] = "L3 subcache in error bit 1",
	[10] = "L3 subcache in error bit 0",
	[9] = "sublink or DRAM channel",
	[8] = "error found by scrub",
	/* 7-4 ht link number of error */
	[3] = "err cpu3",
	[2] = "err cpu2",
	[1] = "err cpu1",
	[0] = "err cpu0",
};

#define IGNORE_HIGHBITS		((1 << 31) || (1 << 28) || (1 << 26))

static void decode_k8_generic_errcode(struct mce_event *e)
{
	char tmp_buf[4096];
	unsigned short errcode = e->status & 0xffff;
	int n;

	/* Translate the highest bits */
	n = bitfield_msg(tmp_buf, sizeof(tmp_buf), highbits, 32,
			 IGNORE_HIGHBITS, 32, e->status);
	if (n)
		mce_snprintf(e->error_msg, "(%s) ", tmp_buf);

	if ((errcode & 0xfff0) == 0x0010)
		mce_snprintf(e->error_msg,
			     "LB error '%s transaction, level %s'",
			     transaction[(errcode >> 2) & 3],
			     cachelevel[errcode & 3]);
	else if ((errcode & 0xff00) == 0x0100)
		mce_snprintf(e->error_msg,
			     "memory/cache error '%s mem transaction, %s transaction, level %s'",
			     memtrans[(errcode >> 4) & 0xf],
			     transaction[(errcode >> 2) & 3],
			     cachelevel[errcode & 3]);
	else if ((errcode & 0xf800) == 0x0800)
		mce_snprintf(e->error_msg,
			     "bus error '%s, %s: %s mem transaction, %s access, level %s'",
			     partproc[(errcode >> 9) & 0x3],
			     timeout[(errcode >> 8) & 1],
			     memtrans[(errcode >> 4) & 0xf],
			     memoryio[(errcode >> 2) & 0x3],
			     cachelevel[(errcode & 0x3)]);
}

static void decode_k8_dc_mc(struct mce_event *e)
{
	unsigned short exterrcode = (e->status >> 16) & 0x0f;
	unsigned short errcode = e->status & 0xffff;

	if (e->status & (3ULL << 45)) {
		mce_snprintf(e->error_msg,
			     "Data cache ECC error (syndrome %x)",
			      (uint32_t) (e->status >> 47) & 0xff);
		if (e->status & (1ULL << 40))
			mce_snprintf(e->error_msg, "found by scrubber");
	}

	if ((errcode & 0xfff0) == 0x0010)
		mce_snprintf(e->error_msg,
			     "TLB parity error in %s array",
			     (exterrcode == 0) ? "physical" : "virtual");
}

static void decode_k8_ic_mc(struct mce_event *e)
{
	unsigned short exterrcode = (e->status >> 16) & 0x0f;
	unsigned short errcode = e->status & 0xffff;

	if (e->status & (3ULL << 45))
		mce_snprintf(e->error_msg, "Instruction cache ECC error");

	if ((errcode & 0xfff0) == 0x0010)
		mce_snprintf(e->error_msg, "TLB parity error in %s array",
			    (exterrcode == 0) ? "physical" : "virtual");
}

static void decode_k8_bu_mc(struct mce_event *e)
{
	unsigned short exterrcode = (e->status >> 16) & 0x0f;

	if (e->status & (3ULL << 45))
		mce_snprintf(e->error_msg, "L2 cache ECC error");

	mce_snprintf(e->error_msg, "%s array error",
		    !exterrcode ? "Bus or cache" : "Cache tag");
}

static void decode_k8_nb_mc(struct mce_event *e, unsigned *memerr)
{
	unsigned short exterrcode = (e->status >> 16) & 0x0f;

	mce_snprintf(e->error_msg, "Northbridge %s", nbextendederr[exterrcode]);

	switch (exterrcode) {
	case 0:
		*memerr = 1;
		mce_snprintf(e->error_msg, "ECC syndrome = %x",
			    (uint32_t) (e->status >> 47) & 0xff);
		break;
	case 8:
		*memerr = 1;
		mce_snprintf(e->error_msg, "Chipkill ECC syndrome = %x",
			    (uint32_t) ((((e->status >> 24) & 0xff) << 8)
			    | ((e->status >> 47) & 0xff)));
		break;
	case 1:
	case 2:
	case 3:
	case 4:
	case 6:
		mce_snprintf(e->error_msg, "link number = %x",
			     (uint32_t) (e->status >> 36) & 0xf);
		break;
	}
}

static void decode_k8_threashold(struct mce_event *e)
{
	if (e->misc & MCI_THRESHOLD_OVER)
		mce_snprintf(e->error_msg, "Threshold error count overflow");
}

static void bank_name(struct mce_event *e)
{
	const char *s;

	if (e->bank < ARRAY_SIZE(k8bank))
		s = k8bank[e->bank];
	else if (e->bank >= K8_MCE_THRESHOLD_BASE &&
		 e->bank < K8_MCE_THRESHOLD_TOP)
		s = k8threshold[e->bank - K8_MCE_THRESHOLD_BASE];
	else
		return;		/* Use the generic parser for bank */

	mce_snprintf(e->bank_name, "%s (bank=%d)", s, e->bank);
}

int parse_amd_k8_event(struct ras_events *ras, struct mce_event *e)
{
	unsigned ismemerr = 0;

	/* Don't handle GART errors */
	if (e->bank == 4) {
		unsigned short exterrcode = (e->status >> 16) & 0x0f;
		if (exterrcode == 5 && (e->status & (1ULL << 61))) {
			return -1;
		}
	}

	bank_name(e);

	switch (e->bank) {
	case 0:
		decode_k8_dc_mc(e);
		decode_k8_generic_errcode(e);
		break;
	case 1:
		decode_k8_ic_mc(e);
		decode_k8_generic_errcode(e);
		break;
	case 2:
		decode_k8_bu_mc(e);
		decode_k8_generic_errcode(e);
		break;
	case 3:		/* LS */
		decode_k8_generic_errcode(e);
		break;
	case 4:
		decode_k8_nb_mc(e, &ismemerr);
		decode_k8_generic_errcode(e);
		break;
	case 5:		/* FR */
		decode_k8_generic_errcode(e);
		break;
	case K8_MCE_THRESHOLD_BASE ... K8_MCE_THRESHOLD_TOP:
		decode_k8_threashold(e);
		break;
	default:
		strcpy(e->error_msg, "Don't know how to decode this bank");
	}

	/* IP doesn't matter on memory errors */
	if (ismemerr)
		e->ip = 0;

	return 0;
}
