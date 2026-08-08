/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2026 Jingyu Li <joey.li@spacemit.com>
 *
 * RISC-V RAS Error Report Register Interface (RERI) handler
 */

#ifndef __RAS_RERI_HANDLER_H
#define __RAS_RERI_HANDLER_H

#include <traceevent/event-parse.h>
#include "ras-events.h"

/* RERI Transaction Types */
#define RERI_TT_UNSPECIFIED		0
#define RERI_TT_CUSTOM			1
#define RERI_TT_RES1			2
#define RERI_TT_RES2			3
#define RERI_TT_EXPLICIT_READ		4
#define RERI_TT_EXPLICIT_WRITE		5
#define RERI_TT_IMPLICIT_READ		6
#define RERI_TT_IMPLICIT_WRITE		7

/* RERI Error Codes */
#define RERI_EC_NONE			0	/* No error */
#define RERI_EC_OUE			1	/* Other unspecified error */
#define RERI_EC_CDA			2	/* Corrupted data access */
#define RERI_EC_CBA			3	/* Cache block data error */
#define RERI_EC_CSD			4	/* Cache scrubbing detected */
#define RERI_EC_CAS			5	/* Cache address/state error */
#define RERI_EC_CUE			6	/* Cache unspecified error */
#define RERI_EC_SDC			7	/* Snoop-filter/directory address/control state */
#define RERI_EC_SUE			8	/* Snoop-filter/directory unspecified error */
#define RERI_EC_TPD			9	/* TLB/Page-walk cache data */
#define RERI_EC_TPA			10	/* TLB/Page-walk address control state */
#define RERI_EC_TPU			11	/* TLB/Page-walk unknown error */
#define RERI_EC_HSE			12	/* Hart state error */
#define RERI_EC_ICS			13	/* Interrupt controller state */
#define RERI_EC_ITD			14	/* Interconnect data error */
#define RERI_EC_ITO			15	/* Interconnection other error */
#define RERI_EC_IWE			16	/* Internal watchdog error */
#define RERI_EC_IDE			17	/* Internal datapath/memory or execution unit error */
#define RERI_EC_SBE			18	/* System memory command or address bus error */
#define RERI_EC_SMU			19	/* System memory unspecified error */
#define RERI_EC_SMD			20	/* System memory data error */
#define RERI_EC_SMS			21	/* System memory scrubbing detected error */
#define RERI_EC_PIO			22	/* Protocol error illegal IO */
#define RERI_EC_PUS			23	/* Protocol error unexpected state */
#define RERI_EC_PTO			24	/* Protocol error timeout error */
#define RERI_EC_SIC			25	/* System internal controller error */
#define RERI_EC_DPU			26	/* Deferred error passthrough not supported */
#define RERI_EC_PCX			27	/* PCI/CXL detected error */
/* 28-63: Reserved for future use */
/* 64-255: Custom/implementation-specific error codes */

/* RERI Address Information Types */
#define RERI_AIT_NONE			0
#define RERI_AIT_SPA			1	/* Supervisor Physical Address */
#define RERI_AIT_GPA			2	/* Guest Physical Address */
#define RERI_AIT_VA			3	/* Virtual Address */

/* RERI Source Types */
enum reri_source_type {
	RERI_SOURCE_TYPE_CPU = 0,
	RERI_SOURCE_TYPE_IOMMU,
	RERI_SOURCE_TYPE_UNKNOWN,
};

/* RERI Error Severity */
enum reri_severity {
	RERI_SEV_INFORMATIONAL = 0,
	RERI_SEV_CORRECTED,
	RERI_SEV_RECOVERABLE,
	RERI_SEV_FATAL,
};

/* RERI Error Record */
struct reri_error_record {
	uint64_t status;
	uint64_t addr_info;
	uint64_t info;
	uint64_t suppl_info;
	uint64_t timestamp;
	uint16_t err_src_id;
	uint8_t source_type;
	uint8_t severity;
	uint32_t hart_id;		/* For CPU errors */
	uint32_t cluster_id;		/* For CPU errors */
};

/* RERI Event from tracepoint */
struct ras_reri_event {
	char timestamp[64];
	uint16_t err_src_id;
	uint8_t source_type;
	uint8_t severity;
	uint32_t hart_id;
	uint32_t cluster_id;
	uint64_t status;
	uint64_t addr_info;
	uint64_t info;
	uint64_t suppl_info;
	uint64_t timestamp_val;
};

int ras_reri_event_handler(struct trace_seq *s,
			   struct tep_record *record,
			   struct tep_event *event,
			   void *context);

#endif /* __RAS_RERI_HANDLER_H */
