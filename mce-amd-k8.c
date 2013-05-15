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

static char *bank_name(unsigned bank)
{
	static char buf[64];
	char *s = "unknown";
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
	trace_seq_printf(s, "%s ",bank_name(e->bank));
	trace_seq_printf(s, "mcgcap= %d ", e->mcgcap);
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

