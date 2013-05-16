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
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include "libtrace/kbuffer.h"
#include "ras-mce-handler.h"
#include "ras-record.h"
#include "ras-logger.h"

/*
 * The code below were adapted from Andi Kleen/Intel/SuSe mcelog code,
 * released under GNU Public General License, v.2
 */
static char *cputype_name[] = {
	[CPU_GENERIC] = "generic CPU",
	[CPU_P6OLD] = "Intel PPro/P2/P3/old Xeon",
	[CPU_CORE2] = "Intel Core", /* 65nm and 45nm */
	[CPU_K8] = "AMD K8 and derivates",
	[CPU_P4] = "Intel P4",
	[CPU_NEHALEM] = "Intel Xeon 5500 series / Core i3/5/7 (\"Nehalem/Westmere\")",
	[CPU_DUNNINGTON] = "Intel Xeon 7400 series",
	[CPU_TULSA] = "Intel Xeon 7100 series",
	[CPU_INTEL] = "Intel generic architectural MCA",
	[CPU_XEON75XX] = "Intel Xeon 7500 series",
	[CPU_SANDY_BRIDGE] = "Sandy Bridge",		/* Fill in better name */
	[CPU_SANDY_BRIDGE_EP] = "Sandy Bridge EP",	/* Fill in better name */
	[CPU_IVY_BRIDGE] = "Ivy Bridge",		/* Fill in better name */
	[CPU_IVY_BRIDGE_EPEX] = "Ivy Bridge EP/EX",	/* Fill in better name */
};

enum cputype select_intel_cputype(struct ras_events *ras)
{
	struct mce_priv *mce = ras->mce_priv;

	if (mce->family == 15) {
		if (mce->model == 6)
			return CPU_TULSA;
		return CPU_P4;
	}
	if (mce->family == 6) {
		if (mce->model >= 0x1a && mce->model != 28)
			mce->mc_error_support = 1;

		if (mce->model < 0xf)
			return CPU_P6OLD;
		else if (mce->model == 0xf || mce->model == 0x17) /* Merom/Penryn */
			return CPU_CORE2;
		else if (mce->model == 0x1d)
			return CPU_DUNNINGTON;
		else if (mce->model == 0x1a || mce->model == 0x2c ||
			 mce->model == 0x1e || mce->model == 0x25)
			return CPU_NEHALEM;
		else if (mce->model == 0x2e || mce->model == 0x2f)
			return CPU_XEON75XX;
		else if (mce->model == 0x2a || mce->model == 0x3a)
			return CPU_SANDY_BRIDGE;
		else if (mce->model == 0x2d)
			return CPU_SANDY_BRIDGE_EP;
		else if (mce->model == 0x3a)
			return CPU_IVY_BRIDGE;
		else if (mce->model == 0x3e)
			return CPU_IVY_BRIDGE_EPEX;
		if (mce->model > 0x1a) {
			log(ALL, LOG_INFO,
			    "Family 6 Model %x CPU: only decoding architectural errors\n",
			    mce->model);
			return CPU_INTEL;
		}
	}
	if (mce->family > 6) {
		log(ALL, LOG_INFO,
		    "Family %u Model %x CPU: only decoding architectural errors\n",
		    mce->family, mce->model);
		return CPU_INTEL;
	}
	log(ALL, LOG_INFO,
	    "Unknown Intel CPU type Family %x Model %x\n",
            mce->family, mce->model);
	return mce->family == 6 ? CPU_P6OLD : CPU_GENERIC;
}

int detect_cpu(struct ras_events *ras)
{
	struct mce_priv *mce = ras->mce_priv;
	FILE *f;
	int ret = 0;
	char *line = NULL;
	size_t linelen = 0;
	enum {
		VENDOR = 1,
		FAMILY = 2,
		MODEL = 4,
		MHZ = 8,
		FLAGS = 16,
		ALL_FLAGS = 0x1f
	} seen = 0;

	mce->family = 0;
	mce->model = 0;
	mce->mhz = 0;
	mce->vendor[0] = '\0';

	f = fopen("/proc/cpuinfo","r");
	if (!f) {
		log(ALL, LOG_INFO, "Can't open /proc/cpuinfo\n");
		return errno;
	}

	while (getdelim(&line, &linelen, '\n', f) > 0 && seen != ALL) {
		if (sscanf(line, "mce->vendor_id : %63[^\n]", &mce->vendor) == 1)
			seen |= VENDOR;
		if (sscanf(line, "cpu mce->family : %d", &mce->family) == 1)
			seen |= FAMILY;
		if (sscanf(line, "mce->model : %d", &mce->model) == 1)
			seen |= MODEL;
		if (sscanf(line, "cpu MHz : %lf", &mce->mhz) == 1)
			seen |= MHZ;
		if (!strncmp(line, "flags", 5) && isspace(line[6])) {
			if (mce->processor_flags)
				free(mce->processor_flags);
			mce->processor_flags = line;
			line = NULL;
			linelen = 0;
			seen |= ALL_FLAGS;
		}
	}

	if (!(seen == ALL)) {
		log(ALL, LOG_INFO, "Can't parse /proc/cpuinfo\n");
		ret = EINVAL;
		goto ret;
	}

	/* Handle only Intel and AMD CPUs */
	ret = 0;

	if (!strcmp(mce->vendor, "AuthenticAMD")) {
		if (mce->family == 15)
			mce->cputype = CPU_K8;
		if (mce->family > 15) {
			log(ALL, LOG_INFO,
			    "Can't parse MCE for this AMD CPU yet\n");
			ret = EINVAL;
		}
		goto ret;
	} else if (!strcmp(mce->vendor,"GenuineIntel")) {
		mce->cputype = select_intel_cputype(ras);
	} else {
		ret = EINVAL;
	}

ret:
	fclose(f);
	free(line);

	return ret;
}

int register_mce_handler(struct ras_events *ras)
{
	int rc;
	struct mce_priv *mce;

	ras->mce_priv = calloc(1, sizeof(struct mce_priv));
	if (!ras->mce_priv) {
		log(ALL, LOG_INFO, "Can't allocate memory MCE data\n");
		return ENOMEM;
	}

	rc = detect_cpu(ras);
	if (rc) {
		mce = ras->mce_priv;
		if (mce->processor_flags)
			free (mce->processor_flags);
		free (ras->mce_priv);
		ras->mce_priv = NULL;
		return (rc);
	}

	return rc;
}

/*
 * End of mcelog's code
 */

unsigned bitfield_msg(char *buf, size_t len, char **bitarray, unsigned array_len,
		      unsigned bit_offset, unsigned ignore_bits,
		      uint64_t status)
{
	int i, n;
	char *p = buf;

	len--;

	for (i = 0; i < array_len; i++) {
		if (status & ignore_bits)
			continue;
		if (status & (1 <<  (i + bit_offset))) {
			if (p != buf) {
				n = snprintf(p, len, ", ");
				len -= n;
				p += n;
			}
			if (!bitarray[i])
				n = snprintf(p, len, "BIT%d", i + bit_offset);
			else
				n = snprintf(p, len, "%s", bitarray[i]);
			len -= n;
			p += n;
		}
	}

	*p = 0;
	return p - buf;
}

static void report_mce_event(struct ras_events *ras,
			     struct pevent_record *record,
			     struct trace_seq *s, struct mce_event *e)
{
	unsigned long long val;
	time_t now;
	struct tm *tm;
	struct mce_priv *mce = ras->mce_priv;

	/*
	 * Newer kernels (3.10-rc1 or upper) provide an uptime clock.
	 * On previous kernels, the way to properly generate an event would
	 * be to inject a fake one, measure its timestamp and diff it against
	 * gettimeofday. We won't do it here. Instead, let's use uptime,
	 * falling-back to the event report's time, if "uptime" clock is
	 * not available (legacy kernels).
	 */

	if (ras->use_uptime)
		now = record->ts/1000000000L + ras->uptime_diff;
	else
		now = time(NULL);

	tm = localtime(&now);
	if (tm)
		strftime(e->timestamp, sizeof(e->timestamp),
			 "%Y-%m-%d %H:%M:%S %z", tm);
	trace_seq_printf(s, "%s ", e->timestamp);

	if (*e->bank_name)
		trace_seq_printf(s, "%s", e->bank_name);
	else
		trace_seq_printf(s, "bank=%x", e->bank);

	trace_seq_printf(s, ", status= %d ", e->status);
	if (*e->error_msg)
		trace_seq_printf(s, ", %s ", e->error_msg);

#if 0
	/*
	 * While the logic for decoding tsc is there at mcelog, why to
	 * decode/print it, if we already got the uptime from the
	 * tracing event? Let's just discard it for now.
	 */
	trace_seq_printf(s, ", tsc= %d ", e->tsc);
	trace_seq_printf(s, ", walltime= %d ", e->walltime);
#endif

	trace_seq_printf(s, "CPU: %s, ", cputype_name[mce->cputype]);
	trace_seq_printf(s, ", cpu= %d ", e->cpu);
	trace_seq_printf(s, ", socketid= %d ", e->socketid);

#if 0
	/*
	 * The CPU vendor is already reported from mce->cputype
	 */
	trace_seq_printf(s, ", cpuvendor= %d", e->cpuvendor);
	trace_seq_printf(s, ", cpuid= %d ", e->cpuid);
#endif

	if (e->ip)
		trace_seq_printf(s, ", ip= %d%s ",
				 !(e->mcgstatus & MCG_STATUS_EIPV) ? " (INEXACT)" : "",
				 e->ip);

	if (e->cs)
		trace_seq_printf(s, ", cs= %d ", e->cs);

	if (e->status & MCI_STATUS_MISCV)
		trace_seq_printf(s, ", misc= %d ", e->misc);

	if (e->status & MCI_STATUS_ADDRV)
		trace_seq_printf(s, ", addr= %d ", e->addr);

	trace_seq_printf(s, ", mcgstatus= %d ", e->mcgstatus);

	if (e->mcgcap)
		trace_seq_printf(s, ", mcgcap= %d ", e->mcgcap);

	trace_seq_printf(s, ", apicid= %d ", e->apicid);

	/*
	 * FIXME: The original mcelog userspace tool uses DMI to map from
	 * address to DIMM. From the comments there, the code there doesn't
	 * take interleaving sets into account. Also, it is known that
	 * BIOS is generally not reliable enough to associate DIMM labels
	 * with addresses.
	 * As, in thesis, we shouldn't be receiving memory error reports via
	 * MCE, as they should go via EDAC traces, let's not do it.
	 */
}

int ras_mce_event_handler(struct trace_seq *s,
			  struct pevent_record *record,
			  struct event_format *event, void *context)
{
	unsigned long long val;
	struct ras_events *ras = context;
	struct mce_priv *mce = ras->mce_priv;
	struct mce_event e;
	int rc;

	memset(&e, 0, sizeof(e));

	/* Parse the MCE error data */
	if (pevent_get_field_val(s, event, "mcgcap", record, &val, 1) < 0)
		return -1;
	e.mcgcap = val;
	if (pevent_get_field_val(s, event, "mcgstatus", record, &val, 1) < 0)
		return -1;
	e.mcgstatus = val;
	if (pevent_get_field_val(s, event, "status", record, &val, 1) < 0)
		return -1;
	e.status = val;
	if (pevent_get_field_val(s, event, "addr", record, &val, 1) < 0)
		return -1;
	e.addr = val;
	if (pevent_get_field_val(s, event, "misc", record, &val, 1) < 0)
		return -1;
	e.misc = val;
	if (pevent_get_field_val(s, event, "ip", record, &val, 1) < 0)
		return -1;
	e.ip = val;
	if (pevent_get_field_val(s, event, "tsc", record, &val, 1) < 0)
		return -1;
	e.tsc = val;
	if (pevent_get_field_val(s, event, "walltime", record, &val, 1) < 0)
		return -1;
	e.walltime = val;
	if (pevent_get_field_val(s, event, "cpu", record, &val, 1) < 0)
		return -1;
	e.cpu = val;
	if (pevent_get_field_val(s, event, "cpuid", record, &val, 1) < 0)
		return -1;
	e.cpuid = val;
	if (pevent_get_field_val(s, event, "apicid", record, &val, 1) < 0)
		return -1;
	e.apicid = val;
	if (pevent_get_field_val(s, event, "socketid", record, &val, 1) < 0)
		return -1;
	e.socketid = val;
	if (pevent_get_field_val(s, event, "cs", record, &val, 1) < 0)
		return -1;
	e.cs = val;
	if (pevent_get_field_val(s, event, "bank", record, &val, 1) < 0)
		return -1;
	e.bank = val;
	if (pevent_get_field_val(s, event, "cpuvendor", record, &val, 1) < 0)
		return -1;
	e.cpuvendor = val;

	switch (mce->cputype) {
	case CPU_GENERIC:
		break;
	case CPU_K8:
		rc = parse_amd_k8_event(ras, &e);
		break;
	default:			/* All other CPU types are Intel */
		rc = parse_intel_event(ras, &e);
	}

	if (rc)
		return rc;

	report_mce_event(ras, record, s, &e);
}
