/*
 * Copyright (C) 2013 Mauro Carvalho Chehab <mchehab+redhat@kernel.org>
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

#ifndef __RAS_MCE_HANDLER_H
#define __RAS_MCE_HANDLER_H

#include <stdint.h>

#include "ras-events.h"
#include <traceevent/event-parse.h>

enum cputype {
	CPU_GENERIC,
	CPU_P6OLD,
	CPU_CORE2, /* 65nm and 45nm */
	CPU_K8,
	CPU_P4,
	CPU_NEHALEM,
	CPU_DUNNINGTON,
	CPU_TULSA,
	CPU_INTEL, /* Intel architectural errors */
	CPU_XEON75XX,
	CPU_SANDY_BRIDGE,
	CPU_SANDY_BRIDGE_EP,
	CPU_IVY_BRIDGE,
	CPU_IVY_BRIDGE_EPEX,
	CPU_HASWELL,
	CPU_HASWELL_EPEX,
	CPU_BROADWELL,
	CPU_BROADWELL_DE,
	CPU_BROADWELL_EPEX,
	CPU_KNIGHTS_LANDING,
	CPU_KNIGHTS_MILL,
	CPU_SKYLAKE_XEON,
	CPU_AMD_SMCA,
	CPU_DHYANA,
	CPU_ICELAKE_XEON,
	CPU_ICELAKE_DE,
	CPU_TREMONT_D,
	CPU_SAPPHIRERAPIDS,
	CPU_EMERALDRAPIDS,
};

struct mce_event {
	/* Unparsed data, obtained directly from MCE tracing */
	uint64_t	mcgcap;
	uint64_t	mcgstatus;
	uint64_t	status;
	uint64_t	addr;
	uint64_t	misc;
	uint64_t	ip;
	uint64_t	tsc;
	uint64_t	walltime;
	uint32_t	cpu;
	uint32_t	cpuid;
	uint32_t	apicid;
	uint32_t	socketid;
	uint8_t		cs;
	uint8_t		bank;
	uint8_t		cpuvendor;
	uint64_t        synd;   /* MCA_SYND MSR: only valid on SMCA systems */
	uint64_t        ipid;   /* MCA_IPID MSR: only valid on SMCA systems */
	uint64_t	ppin;
	uint32_t	microcode;
	int32_t		vdata_len;
	const uint64_t	*vdata;

	/* Parsed data */
	char		frutext[17];
	char		timestamp[64];
	char		bank_name[64];
	char		error_msg[4096];
	char		mcgstatus_msg[256];
	char		mcistatus_msg[1024];
	char		mcastatus_msg[1024];
	char		user_action[4096];
	char		mc_location[256];
};

struct mce_priv {
	/* CPU Info */
	char vendor[64];
	unsigned int family, model;
	double mhz;
	enum cputype cputype;
	unsigned mc_error_support:1;
	char *processor_flags;
};

#define mce_snprintf(buf, fmt, arg...) do {			\
	unsigned int __n = strlen(buf);				\
	unsigned int __len = sizeof(buf) - __n;			\
	if (!__len)						\
		break;						\
	if (__n) {						\
		snprintf(buf + __n, __len, " ");		\
		__len--;					\
		__n++;						\
	}							\
	snprintf(buf + __n, __len, fmt,  ##arg);		\
} while (0)

/* register and handling routines */
int register_mce_handler(struct ras_events *ras, unsigned int ncpus);
int ras_mce_event_handler(struct trace_seq *s,
			  struct tep_record *record,
			  struct tep_event *event, void *context);

/* enables intel iMC logs */
int set_intel_imc_log(enum cputype cputype, unsigned int ncpus);

/* Undertake AMD SMCA Error Decoding */
void decode_smca_error(struct mce_event *e, struct mce_priv *m);
void amd_decode_errcode(struct mce_event *e);

/* Per-CPU-type decoders for Intel CPUs */
void p4_decode_model(struct mce_event *e);
void core2_decode_model(struct mce_event *e);
void p6old_decode_model(struct mce_event *e);
void nehalem_decode_model(struct mce_event *e);
void xeon75xx_decode_model(struct mce_event *e);
void dunnington_decode_model(struct mce_event *e);
void snb_decode_model(struct ras_events *ras, struct mce_event *e);
void ivb_decode_model(struct ras_events *ras, struct mce_event *e);
void hsw_decode_model(struct ras_events *ras, struct mce_event *e);
void knl_decode_model(struct ras_events *ras, struct mce_event *e);
void tulsa_decode_model(struct mce_event *e);
void broadwell_de_decode_model(struct ras_events *ras, struct mce_event *e);
void broadwell_epex_decode_model(struct ras_events *ras, struct mce_event *e);
void skylake_s_decode_model(struct ras_events *ras, struct mce_event *e);
void i10nm_decode_model(enum cputype cputype, struct ras_events *ras, struct mce_event *e);

/* AMD error code decode function */
void decode_amd_errcode(struct mce_event *e);

/* Software defined banks */
#define MCE_EXTENDED_BANK	128

#define MCI_THRESHOLD_OVER  BIT_ULL(48)  /* threshold error count overflow */

#define MCI_STATUS_VAL   BIT_ULL(63)  /* valid error */
#define MCI_STATUS_OVER  BIT_ULL(62)  /* previous errors lost */
#define MCI_STATUS_UC    BIT_ULL(61)  /* uncorrected error */
#define MCI_STATUS_EN    BIT_ULL(60)  /* error enabled */
#define MCI_STATUS_MISCV BIT_ULL(59)  /* misc error reg. valid */
#define MCI_STATUS_ADDRV BIT_ULL(58)  /* addr reg. valid */
#define MCI_STATUS_PCC   BIT_ULL(57)  /* processor context corrupt */
#define MCI_STATUS_S	 BIT_ULL(56)  /* signalled */
#define MCI_STATUS_AR	 BIT_ULL(55)  /* action-required */

/* AMD-specific bits */
#define MCI_STATUS_TCC          BIT_ULL(55)  /* Task context corrupt */
#define MCI_STATUS_SYNDV        BIT_ULL(53)  /* synd reg. valid */
/* uncorrected error,deferred exception */
#define MCI_STATUS_DEFERRED     BIT_ULL(44)
#define MCI_STATUS_POISON       BIT_ULL(43)  /* access poisonous data */

#define MCG_STATUS_RIPV  BIT_ULL(0)   /* restart ip valid */
#define MCG_STATUS_EIPV  BIT_ULL(1)   /* eip points to correct instruction */
#define MCG_STATUS_MCIP  BIT_ULL(2)   /* machine check in progress */
#define MCG_STATUS_LMCE  BIT_ULL(3)   /* local machine check signaled */

/* Those functions are defined on per-cpu vendor C files */
int parse_intel_event(struct ras_events *ras, struct mce_event *e);

int parse_amd_k8_event(struct ras_events *ras, struct mce_event *e);

int parse_amd_smca_event(struct ras_events *ras, struct mce_event *e);

#endif
