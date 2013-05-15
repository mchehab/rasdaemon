/*
 * Copyright (C) 2013 Mauro Carvalho Chehab <mchehab@redhat.com>
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

#ifndef __RAS_AER_HANDLER_H
#define __RAS_AER_HANDLER_H

#include <stdint.h>

#include "ras-events.h"
#include "libtrace/event-parse.h"

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
};

struct mce_event {
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

int register_mce_handler(struct ras_events *ras);
int ras_mce_event_handler(struct trace_seq *s,
			  struct pevent_record *record,
			  struct event_format *event, void *context);

#endif
