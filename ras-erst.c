// SPDX-License-Identifier: GPL-2.0-or-later

/*
 * Copyright (C) 2025 Alibaba Inc
 */

#include <dirent.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

#include "ras-events.h"
#include "ras-erst.h"
#include "ras-logger.h"
#include "ras-mce-handler.h"
#include "ras-record.h"
#include "types.h"

struct mce {
	uint64_t status;		/* Bank's MCi_STATUS MSR */
	uint64_t misc;		/* Bank's MCi_MISC MSR */
	uint64_t addr;		/* Bank's MCi_ADDR MSR */
	uint64_t mcgstatus;	/* Machine Check Global Status MSR */
	uint64_t ip;		/* Instruction Pointer when the error happened */
	uint64_t tsc;		/* CPU time stamp counter */
	uint64_t time;		/* Wall time_t when error was detected */
	uint8_t  cpuvendor;	/* Kernel's X86_VENDOR enum */
	uint8_t  inject_flags;	/* Software inject flags */
	uint8_t  severity;		/* Error severity */
	uint8_t  pad;
	uint32_t cpuid;		/* CPUID 1 EAX */
	uint8_t  cs;		/* Code segment */
	uint8_t  bank;		/* Machine check bank reporting the error */
	uint8_t  cpu;		/* CPU number; obsoleted by extcpu */
	uint8_t  finished;		/* Entry is valid */
	uint32_t extcpu;		/* Linux CPU number that detected the error */
	uint32_t socketid;		/* CPU socket ID */
	uint32_t apicid;		/* CPU initial APIC ID */
	uint64_t mcgcap;		/* MCGCAP MSR: machine check capabilities of CPU */
	uint64_t synd;		/* MCA_SYND MSR: only valid on SMCA systems */
	uint64_t ipid;		/* MCA_IPID MSR: only valid on SMCA systems */
	uint64_t ppin;		/* Protected Processor Inventory Number */
	uint32_t microcode;	/* Microcode revision */
};

static int erst_delete;

#define ERST_PATH "/sys/fs/pstore/erst"
#define MCE_ERST_PREFIX "mce-erst"
#define ERST_EVENT_NAME "mce_erst_record"

#ifdef HAVE_MCE
static void ras_erst_mce_handler(struct ras_events *ras, struct mce_event *e)
{
	struct mce_priv *mce = ras->mce_priv;
	struct trace_seq s;
	int rc = 0;

	switch (mce->cputype) {
	case CPU_GENERIC:
		break;
	case CPU_K8:
		rc = parse_amd_k8_event(ras, e);
		break;
	case CPU_AMD_SMCA:
	case CPU_DHYANA:
		rc = parse_amd_smca_event(ras, e);
		break;
	default:			/* All other CPU types are Intel */
		rc = parse_intel_event(ras, e);
	}

	if (rc)
		return;

	mce_snprintf(e->error_msg, "%s", e->mcastatus_msg);

	trace_seq_init(&s);
	trace_seq_printf(&s, "%16s-%-10d [%03d] %s %6.6f %25s: ",
			 "<...>", 0, -1, "....", 0.0f, ERST_EVENT_NAME);

	report_mce_event(ras, NULL, &s, e);
	trace_seq_terminate(&s);
	trace_seq_do_printf(&s);
	printf("\n");
	fflush(stdout);
	trace_seq_destroy(&s);
}

static void handle_erst_mce_file(char *path, struct mce_event *e)
{
	FILE *file;
	struct mce mce;
	struct stat file_stat;

	file = fopen(path, "r");
	if (!file) {
		log(ALL, LOG_ERR, "Failed to open file %s\n", path);
		return;
	}

	if (stat(path, &file_stat) < 0) {
		log(ALL, LOG_ERR, "Failed to stat file %s\n", path);
		goto out;
	}

	if (fread((char *)&mce, 1, sizeof(mce), file) < sizeof(mce)) {
		log(ALL, LOG_ERR, "Failed to read file %s\n", path);
		goto out;
	}

	e->mcgcap = mce.mcgcap;
	e->mcgstatus = mce.mcgstatus;

	e->status = mce.status;
	e->addr = mce.addr;
	e->misc = mce.misc;
	e->synd = mce.synd;
	e->ipid = mce.ipid;
	e->ip = mce.ip;
	e->tsc = mce.tsc;
	e->walltime = mce.time;
	e->cpu = mce.extcpu;
	e->cpuid = mce.cpuid;
	e->apicid = mce.apicid;
	e->socketid = mce.socketid;
	e->cs = mce.cs;
	e->bank = mce.bank;
	e->cpuvendor = mce.cpuvendor;
	e->ppin = mce.ppin;
	e->microcode = mce.microcode;

	if (erst_delete) {
		if (!unlink(path))
			log(ALL, LOG_INFO, "Error deleting file %s\n", path);
		else
			log(ALL, LOG_ERR, "Failed to delete file %s\n", path);
	}

out:
	fclose(file);
}

static void handle_erst_mce(void)
{
	int rc;
	struct ras_events ras = { 0 };
	struct dirent *entry;
	DIR *dir;

	rc = init_mce_priv(&ras);
	if (rc) {
		log(ALL, LOG_INFO, "Can't register mce handler\n");
		return;
	}

	dir = opendir(ERST_PATH);
	if (!dir) {
		log(ALL, LOG_INFO, "Failed to open directory\n");
		return;
	}

	while ((entry = readdir(dir)) != NULL) {
		struct stat path_stat;
		char file_path[MAX_PATH];
		struct mce_event mce = { 0 };

		mce.erst = 1;
		if (strncmp(entry->d_name, MCE_ERST_PREFIX, strlen(MCE_ERST_PREFIX)))
			continue;

		snprintf(file_path, sizeof(file_path), "%s/%s", ERST_PATH, entry->d_name);
		stat(file_path, &path_stat);

		if (S_ISREG(path_stat.st_mode)) {
			handle_erst_mce_file(file_path, &mce);
		} else {
			log(TERM, LOG_ERR, "Unexpected file type\n");
			continue;
		}

		ras_erst_mce_handler(&ras, &mce);
	}

	closedir(dir);
}
#endif
/* ERST just support mce now */
void handle_erst(void)
{
	if (getenv(ERST_DELETE))
		erst_delete = atoi(getenv(ERST_DELETE));

	handle_erst_mce();
}
