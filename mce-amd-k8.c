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

static void decode_k8_generic_errcode(struct ras_events *ras,
				      struct trace_seq *s, struct mce_event *e)
{
	uint64_t status = e->status;
	unsigned short errcode = status & 0xffff;
	int i;

	for (i = 0; i < 32; i++) {
		if (i == 31 || i == 28 || i == 26)
			continue;
		if (highbits[i] && (status & (1ULL << (i + 32)))) {
			trace_seq_printf(s,  "       bit%d = %s\n", i + 32, highbits[i]);
		}
	}

	if ((errcode & 0xfff0) == 0x0010) {
		trace_seq_printf(s,  "  TLB error '%s transaction, level %s'\n",
		       transaction[(errcode >> 2) & 3],
		       cachelevel[errcode & 3]);
	}
	else if ((errcode & 0xff00) == 0x0100) {
		trace_seq_printf(s,  "  memory/cache error '%s mem transaction, %s transaction, level %s'\n",
		       memtrans[(errcode >> 4) & 0xf],
		       transaction[(errcode >> 2) & 3],
		       cachelevel[errcode & 3]);
	}
	else if ((errcode & 0xf800) == 0x0800) {
		trace_seq_printf(s,  "  bus error '%s, %s\n             %s mem transaction\n             %s access, level %s'\n",
		       partproc[(errcode >> 9) & 0x3],
		       timeout[(errcode >> 8) & 1],
		       memtrans[(errcode >> 4) & 0xf],
		       memoryio[(errcode >> 2) & 0x3],
		       cachelevel[(errcode & 0x3)]);
	}
}

static void decode_k8_dc_mc(struct ras_events *ras,
		            struct trace_seq *s, struct mce_event *e)
{
	uint64_t status = e->status;
	unsigned short exterrcode = (status >> 16) & 0x0f;
	unsigned short errcode = status & 0xffff;

	if(status & (3ULL << 45)) {
		trace_seq_printf(s,  "  Data cache ECC error (syndrome %x)",
		       (uint32_t) (status >> 47) & 0xff);
		if(status&(1ULL << 40)) {
			trace_seq_printf(s, " found by scrubber");
		}
		trace_seq_printf(s, "\n");
	}

	if ((errcode & 0xfff0) == 0x0010) {
		trace_seq_printf(s,  "  TLB parity error in %s array\n",
		       (exterrcode == 0) ? "physical" : "virtual");
	}

	decode_k8_generic_errcode(ras, s, e);
}

static void decode_k8_ic_mc(struct ras_events *ras,
			    struct trace_seq *s, struct mce_event *e)
{
	uint64_t status = e->status;
	unsigned short exterrcode = (status >> 16) & 0x0f;
	unsigned short errcode = status & 0xffff;

	if(status & (3ULL << 45)) {
		trace_seq_printf(s, "  Instruction cache ECC error\n");
	}

	if ((errcode & 0xfff0) == 0x0010) {
		trace_seq_printf(s, "  TLB parity error in %s array\n",
		       (exterrcode == 0) ? "physical" : "virtual");
	}

	decode_k8_generic_errcode(ras, s, e);
}

static void decode_k8_bu_mc(struct ras_events *ras,
			    struct trace_seq *s, struct mce_event *e)
{
	uint64_t status = e->status;
	unsigned short exterrcode = (status >> 16) & 0x0f;

	if(status & (3ULL << 45)) {
		trace_seq_printf(s, "  L2 cache ECC error\n");
	}

	trace_seq_printf(s, "  %s array error\n",
	       (exterrcode == 0) ? "Bus or cache" : "Cache tag");

	decode_k8_generic_errcode(ras, s, e);
}

static void decode_k8_ls_mc(struct ras_events *ras,
			    struct trace_seq *s, struct mce_event *e)
{
	decode_k8_generic_errcode(ras, s, e);
}

static void decode_k8_nb_mc(struct ras_events *ras,
			    struct trace_seq *s, struct mce_event *e,
			    unsigned *memerr)
{
	uint64_t status = e->status;
	unsigned short exterrcode = (status >> 16) & 0x0f;

	trace_seq_printf(s, "  Northbridge %s\n", nbextendederr[exterrcode]);

	switch (exterrcode) {
	case 0:
		*memerr = 1;
		trace_seq_printf(s, "  ECC syndrome = %x\n",
		       (uint32_t) (status >> 47) & 0xff);
		break;
	case 8:
		*memerr = 1;
		trace_seq_printf(s, "  Chipkill ECC syndrome = %x\n",
		       (uint32_t) ((((status >> 24) & 0xff) << 8) | ((status >> 47) & 0xff)));
		break;
	case 1:
	case 2:
	case 3:
	case 4:
	case 6:
		trace_seq_printf(s, "  link number = %x\n",
		       (uint32_t) (status >> 36) & 0xf);
		break;
	}

	decode_k8_generic_errcode(ras, s, e);
}

static void decode_k8_fr_mc(struct ras_events *ras,
			    struct trace_seq *s, struct mce_event *e)
{
	decode_k8_generic_errcode(ras, s, e);
}

#if 0
static void decode_k8_threshold(u64 misc)
{
	if (misc & MCI_THRESHOLD_OVER)
		trace_seq_printf(s, "  Threshold error count overflow\n");
}
#endif

static char *bank_name(uint64_t status, unsigned bank)
{
	static char buf[64];
	char *s = "unknown";

	/* Handle GART errors */
	if (bank == 4) {
		unsigned short exterrcode = (status >> 16) & 0x0f;
		if (exterrcode == 5 && (status & (1ULL << 61))) {
			sprintf(buf, "GART error");
			return 0;
		}
	}

	if (bank < ARRAY_SIZE(k8bank))
		s = k8bank[bank];
	else if (bank >= K8_MCE_THRESHOLD_BASE &&
		 bank < K8_MCE_THRESHOLD_TOP)
		s = k8threshold[bank - K8_MCE_THRESHOLD_BASE];
	else {
		sprintf(buf, "bank=%x", bank);
		return buf;
	}
	snprintf(buf, sizeof(buf) - 1, "%s (bank=%d)", s, bank);
	return buf;
}

void dump_amd_k8_event(struct ras_events *ras,
		       struct trace_seq *s, struct mce_event *e)
{
	unsigned unknown_bank = 0;
	unsigned ismemerr = 0;

	trace_seq_printf(s, "%s ",bank_name(e->status, e->bank));

	switch (e->bank) {
	case 0:
		decode_k8_dc_mc(ras, s, e);
		break;
	case 1:
		decode_k8_ic_mc(ras, s, e);
		break;
	case 2:
		decode_k8_bu_mc(ras, s, e);
		break;
	case 3:
		decode_k8_ls_mc(ras, s, e);
		break;
	case 4:
		decode_k8_nb_mc(ras, s, e, &ismemerr);
		break;
	case 5:
		decode_k8_fr_mc(ras, s, e);
		break;
	default:
		trace_seq_printf(s, "Don't know how to decode this bank");
		unknown_bank = 1;
	}
	trace_seq_printf(s, ", mcgcap= %d ", e->mcgcap);
	trace_seq_printf(s, ", mcgstatus= %d ", e->mcgstatus);
	trace_seq_printf(s, ", status= %d ", e->status);
	trace_seq_printf(s, ", addr= %d ", e->addr);
	trace_seq_printf(s, ", misc= %d ", e->misc);
	trace_seq_printf(s, ", ip= %d ", e->ip);
	trace_seq_printf(s, ", tsc= %d ", e->tsc);
	trace_seq_printf(s, ", walltime= %d ", e->walltime);
	trace_seq_printf(s, ", cpu= %d ", e->cpu);
	trace_seq_printf(s, ", cpuid= %d ", e->cpuid);
	trace_seq_printf(s, ", apicid= %d ", e->apicid);
	trace_seq_printf(s, ", socketid= %d ", e->socketid);
	trace_seq_printf(s, ", cs= %d ", e->cs);
	trace_seq_printf(s, ", cpuvendor= %d", e->cpuvendor);
}
