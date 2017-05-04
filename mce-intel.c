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

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "ras-logger.h"
#include "ras-mce-handler.h"
#include "bitfield.h"

#define MCE_THERMAL_BANK	(MCE_EXTENDED_BANK + 0)
#define MCE_TIMEOUT_BANK        (MCE_EXTENDED_BANK + 90)

#define TLB_LL_MASK      0x3  /*bit 0, bit 1*/
#define TLB_LL_SHIFT     0x0
#define TLB_TT_MASK      0xc  /*bit 2, bit 3*/
#define TLB_TT_SHIFT     0x2

#define CACHE_LL_MASK    0x3  /*bit 0, bit 1*/
#define CACHE_LL_SHIFT   0x0
#define CACHE_TT_MASK    0xc  /*bit 2, bit 3*/
#define CACHE_TT_SHIFT   0x2
#define CACHE_RRRR_MASK  0xF0 /*bit 4, bit 5, bit 6, bit 7 */
#define CACHE_RRRR_SHIFT 0x4

#define BUS_LL_MASK      0x3  /* bit 0, bit 1*/
#define BUS_LL_SHIFT     0x0
#define BUS_II_MASK      0xc  /*bit 2, bit 3*/
#define BUS_II_SHIFT     0x2
#define BUS_RRRR_MASK    0xF0 /*bit 4, bit 5, bit 6, bit 7 */
#define BUS_RRRR_SHIFT   0x4
#define BUS_T_MASK       0x100 /*bit 8*/
#define BUS_T_SHIFT      0x8
#define BUS_PP_MASK      0x600 /*bit 9, bit 10*/
#define BUS_PP_SHIFT     0x9

#define MCG_TES_P       (1ULL<<11)   /* Yellow bit cache threshold supported */


static char *TT[] = {
	"Instruction",
	"Data",
	"Generic",
	"Unknown"
};

static char *LL[] = {
	"Level-0",
	"Level-1",
	"Level-2",
	"Level-3"
};

static struct {
	uint8_t value;
	char* str;
} RRRR [] = {
	{0, "Generic"},
	{1, "Read"},
	{2, "Write" },
	{3, "Data-Read"},
	{4, "Data-Write"},
	{5, "Instruction-Fetch"},
	{6, "Prefetch"},
	{7, "Eviction"},
	{8, "Snoop"}
};

static char *PP[] = {
	"Local-CPU-originated-request",
	"Responed-to-request",
	"Observed-error-as-third-party",
	"Generic"
};

static char *T[] = {
	"Request-did-not-timeout",
	"Request-timed-out"
};

static char *II[] = {
	"Memory-access",
	"Reserved",
	"IO",
	"Other-transaction"
};

static char *mca_msg[] = {
	[0] = "No Error",
	[1] = "Unclassified",
	[2] = "Microcode ROM parity error",
	[3] = "External error",
	[4] = "FRC error",
	[5] = "Internal parity error",
	[6] = "SMM Handler Code Access Violation",
};

static char *tracking_msg[] = {
	[1] = "green",
	[2] = "yellow",
	[3] ="res3"
};

static const char *arstate[4] = {
	[0] = "UCNA",
	[1] = "AR",
	[2] = "SRAO",
	[3] = "SRAR"
};

static char *mmm_mnemonic[] = {
	"GEN", "RD",   "WR",   "AC",
	"MS",  "RES5", "RES6", "RES7"
};

static char *mmm_desc[] = {
	"Generic undefined request",
	"Memory read error",
	"Memory write error",
	"Address/Command error",
	"Memory scrubbing error",
	"Reserved 5",
	"Reserved 6",
	"Reserved 7"
};

static void decode_memory_controller(struct mce_event *e, uint32_t status)
{
	char channel[30];

	if ((status & 0xf) == 0xf)
		sprintf(channel, "unspecified");
	else
		sprintf(channel, "%u", status & 0xf);

	mce_snprintf(e->error_msg, "MEMORY CONTROLLER %s_CHANNEL%s_ERR",
		    mmm_mnemonic[(status >> 4) & 7], channel);
	mce_snprintf(e->error_msg, "Transaction: %s",
		    mmm_desc[(status >> 4) & 7]);
}

static void decode_termal_bank(struct mce_event *e)
{
	if (e->status & 1) {
		mce_snprintf(e->mcgstatus_msg, "Processor %d heated above trip temperature. Throttling enabled.", e->cpu);
		mce_snprintf(e->user_action, "Please check your system cooling. Performance will be impacted");
	} else {
		mce_snprintf(e->error_msg, "Processor %d below trip temperature. Throttling disabled", e->cpu);
	}
}

static void decode_mcg(struct mce_event *e)
{
	uint64_t mcgstatus = e->mcgstatus;

	mce_snprintf(e->mcgstatus_msg, "mcgstatus=%lld",
		     (long long)e->mcgstatus);

	if (mcgstatus & MCG_STATUS_RIPV)
		mce_snprintf(e->mcgstatus_msg, "RIPV");
	if (mcgstatus & MCG_STATUS_EIPV)
		mce_snprintf(e->mcgstatus_msg, "EIPV");
	if (mcgstatus & MCG_STATUS_MCIP)
		mce_snprintf(e->mcgstatus_msg, "MCIP");
	if (mcgstatus & MCG_STATUS_LMCE)
		mce_snprintf(e->mcgstatus_msg, "LMCE");
}

static void bank_name(struct mce_event *e)
{
	char *buf = e->bank_name;

	switch (e->bank) {
	case MCE_THERMAL_BANK:
		strcpy(buf, "THERMAL EVENT");
		break;
	case MCE_TIMEOUT_BANK:
		strcpy(buf, "Timeout waiting for exception on other CPUs");
		break;
	default:
		break;
	}
}

static char *get_RRRR_str(uint8_t rrrr)
{
	unsigned i;

	for (i = 0; i < ARRAY_SIZE(RRRR); i++) {
		if (RRRR[i].value == rrrr) {
			return RRRR[i].str;
		}
	}

	return "UNKNOWN";
}

#define decode_attr(arr, val) ({				\
	char *__str;						\
	if ((unsigned)(val) >= ARRAY_SIZE(arr))			\
		__str = "UNKNOWN";				\
	else							\
		__str = (arr)[val];				\
	__str;							\
})

static void decode_mca(struct mce_event *e, uint64_t track, int *ismemerr)
{
	uint32_t mca = e->status & 0xffffL;

	if (mca & (1UL << 12)) {
		mce_snprintf(e->mcastatus_msg,
			     "corrected filtering (some unreported errors in same region)");
		mca &= ~(1UL << 12);
	}

	if (mca < ARRAY_SIZE(mca_msg)) {
		mce_snprintf(e->mcastatus_msg, "%s", mca_msg[mca]);
		return;
	}

	if ((mca >> 2) == 3) {
		mce_snprintf(e->mcastatus_msg,
			     "%s Generic memory hierarchy error",
			     decode_attr(LL, mca & 3));
	} else if (test_prefix(4, mca)) {
		mce_snprintf(e->mcastatus_msg, "%s TLB %s Error",
				decode_attr(TT, (mca & TLB_TT_MASK) >> TLB_TT_SHIFT),
				decode_attr(LL, (mca & TLB_LL_MASK) >> TLB_LL_SHIFT));
	} else if (test_prefix(8, mca)) {
		unsigned typenum = (mca & CACHE_TT_MASK) >> CACHE_TT_SHIFT;
		unsigned levelnum = (mca & CACHE_LL_MASK) >> CACHE_LL_SHIFT;
		char *type = decode_attr(TT, typenum);
		char *level = decode_attr(LL, levelnum);
		mce_snprintf(e->mcastatus_msg,
			     "%s CACHE %s %s Error", type, level,
			     get_RRRR_str((mca & CACHE_RRRR_MASK) >>
					      CACHE_RRRR_SHIFT));
#if 0
		/* FIXME: We shouldn't mix parsing with actions */
		if (track == 2)
			run_yellow_trigger(e->cpu, typenum, levelnum, type, level, e->socket);
#endif
	} else if (test_prefix(10, mca)) {
		if (mca == 0x400)
			mce_snprintf(e->mcastatus_msg,
				     "Internal Timer error");
		else
			mce_snprintf(e->mcastatus_msg,
				     "Internal unclassified error: %x",
				     mca);
	} else if (test_prefix(11, mca)) {
		mce_snprintf(e->mcastatus_msg, "BUS %s %s %s %s %s Error",
			     decode_attr(LL, (mca & BUS_LL_MASK) >> BUS_LL_SHIFT),
			     decode_attr(PP, (mca & BUS_PP_MASK) >> BUS_PP_SHIFT),
			     get_RRRR_str((mca & BUS_RRRR_MASK) >> BUS_RRRR_SHIFT),
			     decode_attr(II, (mca & BUS_II_MASK) >> BUS_II_SHIFT),
			     decode_attr(T, (mca & BUS_T_MASK) >> BUS_T_SHIFT));
	} else if (test_prefix(7, mca)) {
		decode_memory_controller(e, mca);
		*ismemerr = 1;
	} else
		mce_snprintf(e->mcastatus_msg, "Unknown Error %x", mca);
}

static void decode_tracking(struct mce_event *e, uint64_t track)
{
	if (track == 1)
		mce_snprintf(e->user_action,
			     "Large number of corrected cache errors. System operating, but might leadto uncorrected errors soon");

	if (track)
		mce_snprintf(e->mcistatus_msg, "Threshold based error status: %s",
			     tracking_msg[track]);
}

static void decode_mci(struct mce_event *e, int *ismemerr)
{
	uint64_t track = 0;

	if (!(e->status & MCI_STATUS_VAL))
		mce_snprintf(e->mcistatus_msg, "MCE_INVALID");

	if (e->status & MCI_STATUS_OVER)
		mce_snprintf(e->mcistatus_msg, "Error_overflow");

	/* FIXME: convert into severity */
	if (e->status & MCI_STATUS_UC)
		mce_snprintf(e->mcistatus_msg, "Uncorrected_error");
	else
		mce_snprintf(e->mcistatus_msg, "Corrected_error");


	if (e->status & MCI_STATUS_EN)
		mce_snprintf(e->mcistatus_msg, "Error_enabled");


	if (e->status & MCI_STATUS_PCC)
		mce_snprintf(e->mcistatus_msg, "Processor_context_corrupt");

	if (e->status & (MCI_STATUS_S|MCI_STATUS_AR))
		mce_snprintf(e->mcistatus_msg, "%s",
			     arstate[(e->status >> 55) & 3]);

	if ((e->mcgcap == 0 || (e->mcgcap & MCG_TES_P)) &&
	    !(e->status & MCI_STATUS_UC)) {
		track = (e->status >> 53) & 3;
		decode_tracking(e, track);
	}

	decode_mca(e, track, ismemerr);
}

int parse_intel_event(struct ras_events *ras, struct mce_event *e)
{
	struct mce_priv *mce = ras->mce_priv;
	int ismemerr;

	bank_name(e);

	if (e->bank == MCE_THERMAL_BANK) {
		decode_termal_bank(e);
		return 0;
	}
	decode_mcg(e);
	decode_mci(e, &ismemerr);

	/* Check if the error is at the memory controller */
	if (((e->status & 0xffff) >> 7) == 1) {
		unsigned corr_err_cnt;

		corr_err_cnt = EXTRACT(e->status, 38, 52);
		mce_snprintf(e->mc_location, "n_errors=%d", corr_err_cnt);
	}

	if (test_prefix(11, (e->status & 0xffffL))) {
		switch(mce->cputype) {
		case CPU_P6OLD:
			p6old_decode_model(e);
			break;
		case CPU_DUNNINGTON:
		case CPU_CORE2:
		case CPU_NEHALEM:
		case CPU_XEON75XX:
			core2_decode_model(e);
			break;
		case CPU_TULSA:
		case CPU_P4:
			p4_decode_model(e);
			break;
		default:
			break;
		}
	}
	switch(mce->cputype) {
	case CPU_NEHALEM:
		nehalem_decode_model(e);
		break;
	case CPU_XEON75XX:
		xeon75xx_decode_model(e);
		break;
	case CPU_DUNNINGTON:
		dunnington_decode_model(e);
		break;
	case CPU_TULSA:
		tulsa_decode_model(e);
		break;
	case CPU_SANDY_BRIDGE:
	case CPU_SANDY_BRIDGE_EP:
		snb_decode_model(ras, e);
		break;
	case CPU_IVY_BRIDGE_EPEX:
		ivb_decode_model(ras, e);
		break;
	case CPU_HASWELL_EPEX:
		hsw_decode_model(ras, e);
		break;
	case CPU_KNIGHTS_LANDING:
	case CPU_KNIGHTS_MILL:
		knl_decode_model(ras, e);
		break;
	case CPU_BROADWELL_DE:
		broadwell_de_decode_model(ras, e);
		break;
	case CPU_BROADWELL_EPEX:
		broadwell_epex_decode_model(ras, e);
		break;
	default:
		break;
	}

	return 0;
}

/*
 * Code to enable iMC logs
 */
static int domsr(int cpu, int msr, int bit)
{
	char fpath[32];
	unsigned long long data;
	int fd;

	sprintf(fpath, "/dev/cpu/%d/msr", cpu);
	fd = open(fpath, O_RDWR);
	if (fd == -1) {
		switch (errno) {
		case ENOENT:
			log(ALL, LOG_ERR,
			    "Warning: cpu %d offline?, imc_log not set\n", cpu);
			return -EINVAL;
		default:
			log(ALL, LOG_ERR,
			    "Cannot open %s to set imc_log\n", fpath);
			return -EINVAL;
		}
	}
	if (pread(fd, &data, sizeof data, msr) != sizeof data) {
		log(ALL, LOG_ERR,
		    "Cannot read MSR_ERROR_CONTROL from %s\n", fpath);
		return -EINVAL;
	}
	data |= bit;
	if (pwrite(fd, &data, sizeof data, msr) != sizeof data) {
		log(ALL, LOG_ERR,
		    "Cannot write MSR_ERROR_CONTROL to %s\n", fpath);
		return -EINVAL;
	}
	if (pread(fd, &data, sizeof data, msr) != sizeof data) {
		log(ALL, LOG_ERR,
		    "Cannot re-read MSR_ERROR_CONTROL from %s\n", fpath);
		return -EINVAL;
	}
	if ((data & bit) == 0) {
		log(ALL, LOG_ERR,
		    "Failed to set imc_log on cpu %d\n", cpu);
		return -EINVAL;
	}
	close(fd);
	return 0;
}

int set_intel_imc_log(enum cputype cputype, unsigned ncpus)
{
	int cpu, msr, bit, rc;

	switch (cputype) {
	case CPU_SANDY_BRIDGE_EP:
	case CPU_IVY_BRIDGE_EPEX:
	case CPU_HASWELL_EPEX:
	case CPU_KNIGHTS_LANDING:
	case CPU_KNIGHTS_MILL:
		msr = 0x17f;	/* MSR_ERROR_CONTROL */
		bit = 0x2;	/* MemError Log Enable */
		break;
	default:
		return 0;
	}

	for (cpu = 0; cpu < ncpus; cpu++) {
		rc = domsr(cpu, msr, bit);
		if (rc)
			return rc;
	}

	return 0;
}
