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

#define K8_MCE_THRESHOLD_BASE        (MCE_EXTENDED_BANK + 1)      /* MCE_AMD */
#define K8_MCE_THRESHOLD_TOP         (K8_MCE_THRESHOLD_BASE + 6 * 9)

#define K8_MCELOG_THRESHOLD_DRAM_ECC (4 * 9 + 0)
#define K8_MCELOG_THRESHOLD_LINK     (4 * 9 + 1)
#define K8_MCELOG_THRESHOLD_L3_CACHE (4 * 9 + 2)
#define K8_MCELOG_THRESHOLD_FBDIMM   (4 * 9 + 3)

static char *k8bank[] = {
	"data cache",
	"instruction cache",
	"bus unit",
	"load/store unit",
	"northbridge",
	"fixed-issue reoder"
};

static char *k8threshold[] = {
	[0 ... K8_MCELOG_THRESHOLD_DRAM_ECC - 1] = "Unknow threshold counter",
	[K8_MCELOG_THRESHOLD_DRAM_ECC] = "MC4_MISC0 DRAM threshold",
	[K8_MCELOG_THRESHOLD_LINK] = "MC4_MISC1 Link threshold",
	[K8_MCELOG_THRESHOLD_L3_CACHE] = "MC4_MISC2 L3 Cache threshold",
	[K8_MCELOG_THRESHOLD_FBDIMM] = "MC4_MISC3 FBDIMM threshold",
	[K8_MCELOG_THRESHOLD_FBDIMM + 1 ...
	 K8_MCE_THRESHOLD_TOP - K8_MCE_THRESHOLD_BASE - 1] =
                "Unknown threshold counter",
};

static char *transaction[] = {
	"instruction", "data", "generic", "reserved"
};
static char *cachelevel[] = {
	"0", "1", "2", "generic"
};
static char *memtrans[] = {
	"generic error", "generic read", "generic write", "data read",
	"data write", "instruction fetch", "prefetch", "evict", "snoop",
	"?", "?", "?", "?", "?", "?", "?"
};
static char *partproc[] = {
	"local node origin", "local node response",
	"local node observed", "generic participation"
};
static char *timeout[] = {
	"request didn't time out",
	"request timed out"
};
static char *memoryio[] = {
	"memory", "res.", "i/o", "generic"
};
static char *nbextendederr[] = {
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
static char *highbits[32] = {
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

static void decode_k8_generic_errcode(uint64_t status, char *buf, size_t *len)
{
	char tmp_buf[4096];
	unsigned short errcode = status & 0xffff;
	int i, n;
	char *p = buf;

	/* Translate the highest bits */
	n = bitfield_msg(tmp_buf, sizeof(*len), highbits, 32, IGNORE_HIGHBITS,
			 32, status);
	if (n) {
		n = snprintf(p, *len, "%s ", tmp_buf);
		p += n;
		*len -= n;
	}

	if ((errcode & 0xfff0) == 0x0010) {
		n = snprintf(p, *len, "LB error '%s transaction, level %s'",
		       transaction[(errcode >> 2) & 3],
		       cachelevel[errcode & 3]);
		p += n;
		*len -= n;
	}
	else if ((errcode & 0xff00) == 0x0100) {
		n = snprintf(p, *len,
			     "memory/cache error '%s mem transaction, %s transaction, level %s'",
			     memtrans[(errcode >> 4) & 0xf],
			     transaction[(errcode >> 2) & 3],
			     cachelevel[errcode & 3]);
		p += n;
		*len -= n;
	}
	else if ((errcode & 0xf800) == 0x0800) {
		n = snprintf(p, *len,
			     "bus error '%s, %s: %s mem transaction, %s access, level %s'",
			     partproc[(errcode >> 9) & 0x3],
			     timeout[(errcode >> 8) & 1],
			     memtrans[(errcode >> 4) & 0xf],
			     memoryio[(errcode >> 2) & 0x3],
			     cachelevel[(errcode & 0x3)]);
		p += n;
		*len -= n;
	}
}

static void decode_k8_dc_mc(uint64_t status, char *buf, size_t *len)
{
	unsigned short exterrcode = (status >> 16) & 0x0f;
	unsigned short errcode = status & 0xffff;
	int n;
	char *p = buf;

	if (status & (3ULL << 45)) {
		n = snprintf(p, *len, "Data cache ECC error (syndrome %x)",
		       (uint32_t) (status >> 47) & 0xff);
		p += n;
		*len -= n;
		if(status & (1ULL << 40)) {
			n = snprintf(p, *len, " found by scrubber");
			p += n;
			*len -= n;
		}
	}

	if ((errcode & 0xfff0) == 0x0010) {
		if (p != buf) {
			n = snprintf(p, *len, " ");
			p += n;
			*len -= n;
		}
		n = snprintf(p, *len, "TLB parity error in %s array",
		       (exterrcode == 0) ? "physical" : "virtual");
		p += n;
		*len -= n;
	}

	if (p != buf) {
		n = snprintf(p, *len, " ");
		p += n;
		*len -= n;
	}

	decode_k8_generic_errcode(status, p, len);
}

static void decode_k8_ic_mc(uint64_t status, char *buf, size_t *len)
{
	unsigned short exterrcode = (status >> 16) & 0x0f;
	unsigned short errcode = status & 0xffff;
	int n;
	char *p = buf;

	if (status & (3ULL << 45)) {
		n = snprintf(p, *len, "Instruction cache ECC error");
		p += n;
		*len -= n;
	}

	if ((errcode & 0xfff0) == 0x0010) {
		if (p != buf) {
			n = snprintf(p, *len, " ");
			p += n;
			*len -= n;
		}
		n = snprintf(p, *len, "TLB parity error in %s array",
		       (exterrcode == 0) ? "physical" : "virtual");
		p += n;
		*len -= n;
	}
	if (p != buf) {
		n = snprintf(p, *len, " ");
		p += n;
		*len -= n;
	}

	decode_k8_generic_errcode(status, p, len);
}

static void decode_k8_bu_mc(uint64_t status, char *buf, size_t *len)
{
	unsigned short exterrcode = (status >> 16) & 0x0f;
	int n;
	char *p = buf;

	if (status & (3ULL << 45)) {
		n = snprintf(p, *len, "L2 cache ECC error");
		p += n;
		*len -= n;
	}

	if (p != buf) {
		n = snprintf(p, *len, " ");
		p += n;
		*len -= n;
	}

	n = snprintf(p, *len, "%s array error",
		    !exterrcode ? "Bus or cache" : "Cache tag");

	if (p != buf) {
		n = snprintf(p, *len, " ");
		p += n;
		*len -= n;
	}

	decode_k8_generic_errcode(status, p, len);
}

static void decode_k8_nb_mc(uint64_t status, char *buf, size_t *len,
			    unsigned *memerr)
{
	unsigned short exterrcode = (status >> 16) & 0x0f;
	int n;
	char *p = buf;

	n = snprintf(buf, *len, "Northbridge %s", nbextendederr[exterrcode]);
	p += n;
	*len -= n;

	n = 0;
	switch (exterrcode) {
	case 0:
		*memerr = 1;
		n = snprintf(p, *len, " ECC syndrome = %x",
			    (uint32_t) (status >> 47) & 0xff);
		break;
	case 8:
		*memerr = 1;
		n = snprintf(p, *len, " Chipkill ECC syndrome = %x",
			    (uint32_t) ((((status >> 24) & 0xff) << 8) | ((status >> 47) & 0xff)));
		break;
	case 1:
	case 2:
	case 3:
	case 4:
	case 6:
		n = snprintf(p, *len, " link number = %x\n",
			     (uint32_t) (status >> 36) & 0xf);
		break;
	}
	p += n;
	*len -= n;

	if (p != buf) {
		n = snprintf(p, *len, " ");
		p += n;
		*len -= n;
	}

	decode_k8_generic_errcode(status, p, len);
}

static void decode_k8_threashold(uint64_t misc, char *buf, size_t *len)
{
	int n;
	char *p = buf;

	if (misc & MCI_THRESHOLD_OVER) {
		n = snprintf(p, *len, "  Threshold error count overflow\n");
		p += n;
		*len -= n;
	}
}

static void bank_name(struct mce_event *e)
{
	char *buf = e->bank_name;
	char *s;

	if (e->bank < ARRAY_SIZE(k8bank))
		s = k8bank[e->bank];
	else if (e->bank >= K8_MCE_THRESHOLD_BASE &&
		 e->bank < K8_MCE_THRESHOLD_TOP)
		s = k8threshold[e->bank - K8_MCE_THRESHOLD_BASE];
	else
		return;		/* Use the generic parser for bank */

	snprintf(buf, sizeof(buf) - 1, "%s (bank=%d)", s, e->bank);
}

int parse_amd_k8_event(struct ras_events *ras, struct mce_event *e)
{
	unsigned unknown_bank = 0;
	unsigned ismemerr = 0;
	char *buf = e->error_msg;
	size_t len = sizeof(e->error_msg);

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
		decode_k8_dc_mc(e->status, buf, &len);
		break;
	case 1:
		decode_k8_ic_mc(e->status, buf, &len);
		break;
	case 2:
		decode_k8_bu_mc(e->status, buf, &len);
		break;
	case 3:		/* LS */
		decode_k8_generic_errcode(e->status, buf, &len);
		break;
	case 4:
		decode_k8_nb_mc(e->status, buf, &len, &ismemerr);
		break;
	case 5:		/* FR */
		decode_k8_generic_errcode(e->status, buf, &len);
		break;
	case K8_MCE_THRESHOLD_BASE ... K8_MCE_THRESHOLD_TOP:
		decode_k8_threashold(e->misc, buf, &len);
		break;
	default:
		strcpy(e->error_msg, "Don't know how to decode this bank");
	}

	/* IP doesn't matter on memory errors */
	if (ismemerr)
		e->ip = 0;

	return 0;
}
