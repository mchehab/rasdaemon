// SPDX-License-Identifier: GPL-2.0-or-later

/*
 * Copyright (C) 2025 Alibaba Inc
 */

#include "config.h"

#include <dirent.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

#include "core/modules.h"
#include "core/ras-events.h"
#include "core/ras-logger.h"
#include "core/types.h"
#include "events-arch-x86/ras-erst.h"
#include "events-arch-x86/ras-mce-handler.h"

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

#define ERST_PATH "/sys/fs/pstore"
#define MCE_ERST_PREFIX "mce-erst"
#define ERST_EVENT_NAME "mce_erst_record"

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

static int handle_erst_mce_file(const char *path, struct mce_event *e)
{
	FILE *file;
	struct mce mce;
	struct stat file_stat;

	file = fopen(path, "r");
	if (!file) {
		log(ALL, LOG_ERR, "Failed to open file %s\n", path);
		return -1;
	}

	if (stat(path, &file_stat) < 0) {
		log(ALL, LOG_ERR, "Failed to stat file %s\n", path);
		fclose(file);
		return -1;
	}

	if (fread((char *)&mce, 1, sizeof(mce), file) < sizeof(mce)) {
		log(ALL, LOG_ERR, "Failed to read file %s\n", path);
		fclose(file);
		return -1;
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
			log(ALL, LOG_INFO, "Deleted file %s\n", path);
		else
			log(ALL, LOG_ERR, "Failed to delete file %s\n", path);
	}

	fclose(file);
	return 0;
}

#ifdef HAVE_UNITTEST
int ras_erst_test_read(const char *path, struct mce_event *event)
{
	return handle_erst_mce_file(path, event);
}
#endif

static void handle_erst_mce(struct ras_events *ras, const char *path)
{
	struct dirent *entry;
	DIR *dir;

	dir = opendir(path);
	if (!dir)
		return;

	while ((entry = readdir(dir)) != NULL) {
		struct stat path_stat;
		char file_path[MAX_PATH];
		struct mce_event mce = { 0 };

		mce.erst = 1;
		if (strncmp(entry->d_name, MCE_ERST_PREFIX,
			    strlen(MCE_ERST_PREFIX)))
			continue;

		snprintf(file_path, sizeof(file_path), "%s/%s",
			 path, entry->d_name);
		if (stat(file_path, &path_stat) < 0) {
			log(ALL, LOG_ERR, "Failed to stat file %s\n",
			    file_path);
			continue;
		}

		if (S_ISREG(path_stat.st_mode)) {
			if (handle_erst_mce_file(file_path, &mce))
				continue;
		} else {
			log(TERM, LOG_ERR, "Unexpected file type\n");
			continue;
		}

		ras_erst_mce_handler(ras, &mce);
	}

	closedir(dir);
}

static int check_mce_pstore(const char **path)
{
	/* Try first the old location as its path is longer */
	if (access(ERST_PATH "/erst", R_OK | X_OK) == 0) {
		*path = ERST_PATH "/erst";
		return 0;
	}

	if (access(ERST_PATH, R_OK | X_OK) == 0) {
		*path = ERST_PATH;
		return 0;
	}

	return -ENOENT;
}

/* ERST just support mce now */
static int ras_erst_init(struct ras_module_ctx *ctx)
{
	const char *path;
	int rc;

	if (choices_disable && *choices_disable &&
	    strstr(choices_disable, "ras:erst")) {
		log(ALL, LOG_INFO, "Disabled ras:erst from config\n");
		return 0;
	}

	if (getenv(ERST_DELETE))
		erst_delete = atoi(getenv(ERST_DELETE));

	rc = check_mce_pstore(&path);
	if (rc)
		return rc;

	rc = init_mce_priv(ctx->ras);
	if (rc) {
		log(ALL, LOG_INFO, "Can't register mce handler\n");
		return rc;
	}

	handle_erst_mce(ctx->ras, path);

	return 0;
}

static const struct ras_module_entry ras_erst_module = {
	.name = "x86-mce-erst",
	.level = SUB_EVENT_MODULE,
	.init = ras_erst_init,
};

REGISTER_RAS_MODULE(ras_erst_module);
