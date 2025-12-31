// SPDX-License-Identifier: GPL-2.0

/*
 * Copyright (c) 2016, The Linux Foundation. All rights reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <traceevent/kbuffer.h>
#include <unistd.h>

#include "non-standard-ampere.h"
#include "ras-arm-handler.h"
#include "ras-cpu-isolation.h"
#include "ras-logger.h"
#include "ras-non-standard-handler.h"
#include "ras-report.h"
#include "types.h"

#define ARM_ERR_VALID_ERROR_COUNT BIT(0)
#define ARM_ERR_VALID_FLAGS BIT(1)
#define BIT2 2

#define ARM_INFO_VALID_MULTI_ERR	BIT(0)
#define ARM_INFO_VALID_FLAGS		BIT(1)
#define ARM_INFO_VALID_ERR_INFO		BIT(2)
#define ARM_INFO_VALID_VIRT_ADDR	BIT(3)
#define ARM_INFO_VALID_PHYSICAL_ADDR	BIT(4)

#define ARM_INFO_FLAGS_FIRST		BIT(0)
#define ARM_INFO_FLAGS_LAST		BIT(1)
#define ARM_INFO_FLAGS_PROPAGATED	BIT(2)
#define ARM_INFO_FLAGS_OVERFLOW		BIT(3)

#define ARM_ERR_TYPE_MASK		GENMASK(4, 1)
#define ARM_CACHE_ERROR			BIT(1)
#define ARM_TLB_ERROR			BIT(2)
#define ARM_BUS_ERROR			BIT(3)
#define ARM_VENDOR_ERROR		BIT(4)

#define ARM_ERR_VALID_TRANSACTION_TYPE		BIT(0)
#define ARM_ERR_VALID_OPERATION_TYPE		BIT(1)
#define ARM_ERR_VALID_LEVEL			BIT(2)
#define ARM_ERR_VALID_PROC_CONTEXT_CORRUPT	BIT(3)
#define ARM_ERR_VALID_CORRECTED			BIT(4)
#define ARM_ERR_VALID_PRECISE_PC		BIT(5)
#define ARM_ERR_VALID_RESTARTABLE_PC		BIT(6)
#define ARM_ERR_VALID_PARTICIPATION_TYPE	BIT(7)
#define ARM_ERR_VALID_TIME_OUT			BIT(8)
#define ARM_ERR_VALID_ADDRESS_SPACE		BIT(9)
#define ARM_ERR_VALID_MEM_ATTRIBUTES		BIT(10)
#define ARM_ERR_VALID_ACCESS_MODE		BIT(11)

#define ARM_ERR_TRANSACTION_SHIFT		16
#define ARM_ERR_TRANSACTION_MASK		GENMASK(1, 0)
#define ARM_ERR_OPERATION_SHIFT			18
#define ARM_ERR_OPERATION_MASK			GENMASK(3, 0)
#define ARM_ERR_LEVEL_SHIFT			22
#define ARM_ERR_LEVEL_MASK			GENMASK(2, 0)
#define ARM_ERR_PC_CORRUPT_SHIFT		25
#define ARM_ERR_PC_CORRUPT_MASK			GENMASK(0, 0)
#define ARM_ERR_CORRECTED_SHIFT			26
#define ARM_ERR_CORRECTED_MASK			GENMASK(0, 0)
#define ARM_ERR_PRECISE_PC_SHIFT		27
#define ARM_ERR_PRECISE_PC_MASK			GENMASK(0, 0)
#define ARM_ERR_RESTARTABLE_PC_SHIFT		28
#define ARM_ERR_RESTARTABLE_PC_MASK		GENMASK(0, 0)
#define ARM_ERR_PARTICIPATION_TYPE_SHIFT	29
#define ARM_ERR_PARTICIPATION_TYPE_MASK		GENMASK(1, 0)
#define ARM_ERR_TIME_OUT_SHIFT			31
#define ARM_ERR_TIME_OUT_MASK			GENMASK(0, 0)
#define ARM_ERR_ADDRESS_SPACE_SHIFT		32
#define ARM_ERR_ADDRESS_SPACE_MASK		GENMASK(1, 0)
#define ARM_ERR_MEM_ATTRIBUTES_SHIFT		34
#define ARM_ERR_MEM_ATTRIBUTES_MASK		GENMASK(8, 0)
#define ARM_ERR_ACCESS_MODE_SHIFT		43
#define ARM_ERR_ACCESS_MODE_MASK		GENMASK(0, 0)

void display_raw_data(struct trace_seq *s,
		      const uint8_t *buf,
		      uint32_t datalen)
{
	int i = 0, line_count = 0;

	trace_seq_printf(s, "  %08x: ", i);
	while (datalen >= 4) {
		print_le_hex(s, buf, i);
		i += 4;
		datalen -= 4;
		if (++line_count == 4) {
			trace_seq_printf(s, "\n  %08x: ", i);
			line_count = 0;
		} else {
			trace_seq_printf(s, " ");
		}
	}
}

static const char * const arm_proc_error_type_strs[] = {
	"",
	"cache error",
	"TLB error",
	"bus error",
	"micro-architectural error",
};

static const char * const arm_proc_error_flags_strs[] = {
	"first error ",
	"last error",
	"propagated error",
	"overflow",
};

static const char * const arm_err_trans_type_strs[] = {
	"Instruction",
	"Data Access",
	"Generic",
};

static const char * const arm_bus_err_op_strs[] = {
	"Generic error (type cannot be determined)",
	"Generic read (type of instruction or data request cannot be determined)",
	"Generic write (type of instruction of data request cannot be determined)",
	"Data read",
	"Data write",
	"Instruction fetch",
	"Prefetch",
};

static const char * const arm_cache_err_op_strs[] = {
	"Generic error (type cannot be determined)",
	"Generic read (type of instruction or data request cannot be determined)",
	"Generic write (type of instruction of data request cannot be determined)",
	"Data read",
	"Data write",
	"Instruction fetch",
	"Prefetch",
	"Eviction",
	"Snooping (processor initiated a cache snoop that resulted in an error)",
	"Snooped (processor raised a cache error caused by another processor or device snooping its cache)",
	"Management",
};

static const char * const arm_tlb_err_op_strs[] = {
	"Generic error (type cannot be determined)",
	"Generic read (type of instruction or data request cannot be determined)",
	"Generic write (type of instruction of data request cannot be determined)",
	"Data read",
	"Data write",
	"Instruction fetch",
	"Prefetch",
	"Local management operation (processor initiated a TLB management operation that resulted in an error)",
	"External management operation (processor raised a TLB error caused by another processor or device broadcasting TLB operations)",
};

static const char * const arm_bus_err_part_type_strs[] = {
	"Local processor originated request",
	"Local processor responded to request",
	"Local processor observed",
	"Generic",
};

static const char * const arm_bus_err_addr_space_strs[] = {
	"External Memory Access",
	"Internal Memory Access",
	"Unknown",
	"Device Memory Access",
};

static int decode_err_data_bits(char *buf, unsigned long data,
				const char **data_str, size_t str_size)
{
	int bit;

	if (!buf || !data_str || !str_size)
		return -1;

	for (bit = 0; bit < str_size; bit++)
		if (data & BIT(bit))
			mce_snprintf(buf, " %s", ((char *)data_str[bit]));
	return 0;
}

static void parse_arm_err_info(struct trace_seq *s, uint32_t type, uint64_t error_info)
{
	uint8_t trans_type, op_type, level, participation_type, address_space;
	uint16_t mem_attributes;
	bool proc_context_corrupt, corrected, precise_pc, restartable_pc;
	bool time_out, access_mode;

	/*
	 * Vendor type errors have error information values that are vendor
	 * specific.
	 */
	if (type & ARM_VENDOR_ERROR)
		return;

	if (error_info & ARM_ERR_VALID_TRANSACTION_TYPE) {
		trans_type = ((error_info >> ARM_ERR_TRANSACTION_SHIFT)
			      & ARM_ERR_TRANSACTION_MASK);
		if (trans_type < ARRAY_SIZE(arm_err_trans_type_strs))
			trace_seq_printf(s, " transaction type:%s",
					 arm_err_trans_type_strs[trans_type]);
	}

	if (error_info & ARM_ERR_VALID_OPERATION_TYPE) {
		op_type = ((error_info >> ARM_ERR_OPERATION_SHIFT)
			   & ARM_ERR_OPERATION_MASK);
		if (type & ARM_CACHE_ERROR) {
			if (op_type < ARRAY_SIZE(arm_cache_err_op_strs))
				trace_seq_printf(s, " cache error, operation type:%s",
						 arm_cache_err_op_strs[op_type]);
		}
		if (type & ARM_TLB_ERROR) {
			if (op_type < ARRAY_SIZE(arm_tlb_err_op_strs)) {
				trace_seq_printf(s, " TLB error, operation type: %s",
						 arm_tlb_err_op_strs[op_type]);
			}
		}
		if (type & ARM_BUS_ERROR) {
			if (op_type < ARRAY_SIZE(arm_bus_err_op_strs)) {
				trace_seq_printf(s, " bus error, operation type: %s",
						 arm_bus_err_op_strs[op_type]);
			}
		}
	}

	if (error_info & ARM_ERR_VALID_LEVEL) {
		level = ((error_info >> ARM_ERR_LEVEL_SHIFT)
			 & ARM_ERR_LEVEL_MASK);
		if (type & ARM_CACHE_ERROR)
			trace_seq_printf(s, " cache level: %d", level);

		if (type & ARM_TLB_ERROR)
			trace_seq_printf(s, " TLB level: %d", level);

		if (type & ARM_BUS_ERROR)
			trace_seq_printf(s, " affinity level at which the bus error occurred: %d",
					 level);
	}

	if (error_info & ARM_ERR_VALID_PROC_CONTEXT_CORRUPT) {
		proc_context_corrupt = ((error_info >> ARM_ERR_PC_CORRUPT_SHIFT)
					& ARM_ERR_PC_CORRUPT_MASK);
		if (proc_context_corrupt)
			trace_seq_printf(s, " processor context corrupted");
		else
			trace_seq_printf(s, " processor context not corrupted");
	}

	if (error_info & ARM_ERR_VALID_CORRECTED) {
		corrected = ((error_info >> ARM_ERR_CORRECTED_SHIFT)
			     & ARM_ERR_CORRECTED_MASK);
		if (corrected)
			trace_seq_printf(s, " the error has been corrected");
		else
			trace_seq_printf(s, " the error has not been corrected");
	}

	if (error_info & ARM_ERR_VALID_PRECISE_PC) {
		precise_pc = ((error_info >> ARM_ERR_PRECISE_PC_SHIFT)
			      & ARM_ERR_PRECISE_PC_MASK);
		if (precise_pc)
			trace_seq_printf(s, " PC is precise");
		else
			trace_seq_printf(s, " PC is imprecise");
	}

	if (error_info & ARM_ERR_VALID_RESTARTABLE_PC) {
		restartable_pc = ((error_info >> ARM_ERR_RESTARTABLE_PC_SHIFT)
				  & ARM_ERR_RESTARTABLE_PC_MASK);
		if (restartable_pc)
			trace_seq_printf(s, " Program execution can be restartable reliably at the PC");
	}

	/* The rest of the fields are specific to bus errors */
	if (type != ARM_BUS_ERROR)
		return;

	if (error_info & ARM_ERR_VALID_PARTICIPATION_TYPE) {
		participation_type = ((error_info >> ARM_ERR_PARTICIPATION_TYPE_SHIFT)
				      & ARM_ERR_PARTICIPATION_TYPE_MASK);
		if (participation_type < ARRAY_SIZE(arm_bus_err_part_type_strs)) {
			trace_seq_printf(s, " participation type: %s",
					 arm_bus_err_part_type_strs[participation_type]);
		}
	}

	if (error_info & ARM_ERR_VALID_TIME_OUT) {
		time_out = ((error_info >> ARM_ERR_TIME_OUT_SHIFT)
			    & ARM_ERR_TIME_OUT_MASK);
		if (time_out)
			trace_seq_printf(s, " request timed out");
	}

	if (error_info & ARM_ERR_VALID_ADDRESS_SPACE) {
		address_space = ((error_info >> ARM_ERR_ADDRESS_SPACE_SHIFT)
				 & ARM_ERR_ADDRESS_SPACE_MASK);
		if (address_space < ARRAY_SIZE(arm_bus_err_addr_space_strs)) {
			trace_seq_printf(s, " address space: %s",
					 arm_bus_err_addr_space_strs[address_space]);
		}
	}

	if (error_info & ARM_ERR_VALID_MEM_ATTRIBUTES) {
		mem_attributes = ((error_info >> ARM_ERR_MEM_ATTRIBUTES_SHIFT)
				  & ARM_ERR_MEM_ATTRIBUTES_MASK);
		trace_seq_printf(s, " memory access attributes:0x%x",
				 mem_attributes);
	}

	if (error_info & ARM_ERR_VALID_ACCESS_MODE) {
		access_mode = ((error_info >> ARM_ERR_ACCESS_MODE_SHIFT)
			       & ARM_ERR_ACCESS_MODE_MASK);
		if (access_mode)
			trace_seq_printf(s, " access mode: normal");
		else
			trace_seq_printf(s, " access mode: secure");
	}
}

static int parse_arm_processor_err_info(struct trace_seq *s, struct ras_arm_event *ev)
{
	int err_info_size = sizeof(struct ras_arm_err_info);
	struct ras_arm_err_info *err_info;
	int i, num_pei;

	if (ev->pei_len % err_info_size != 0) {
		log(TERM, LOG_ERR,
		    "The event data does not match to the ARM Processor Error Information Structure\n");
		return -1;
	}
	num_pei = ev->pei_len / err_info_size;
	err_info = (struct ras_arm_err_info *)(ev->pei_error);

	trace_seq_printf(s, "\nARM processor error info:\n");
	for (i = 0; i < num_pei; ++i) {
		decode_err_data_bits(ev->error_types, err_info->type,
				     (const char **)arm_proc_error_type_strs,
				     ARRAY_SIZE(arm_proc_error_type_strs));
		trace_seq_printf(s, " error_types:%s", ev->error_types);

		if (err_info->validation_bits & ARM_ERR_VALID_ERROR_COUNT) {
			ev->error_count = err_info->multiple_error + 1;
			trace_seq_printf(s, " error_count:%d", ev->error_count);
		}
		if (err_info->validation_bits & ARM_INFO_VALID_FLAGS) {
			decode_err_data_bits(ev->error_flags, err_info->flags,
					     (const char **)arm_proc_error_flags_strs,
					     ARRAY_SIZE(arm_proc_error_flags_strs));
			trace_seq_printf(s, " error_flags:%s", ev->error_flags);
		}
		if (err_info->validation_bits & ARM_INFO_VALID_ERR_INFO) {
			ev->error_info = err_info->error_info;
			trace_seq_printf(s, " error_info: 0x%016llx",
					 (unsigned long long)ev->error_info);
			parse_arm_err_info(s, err_info->type, ev->error_info);
		}
		if (err_info->validation_bits & ARM_INFO_VALID_VIRT_ADDR) {
			ev->virt_fault_addr = err_info->virt_fault_addr;
			trace_seq_printf(s, " virtual fault address: 0x%016llx",
					 (unsigned long long)err_info->virt_fault_addr);
		}
		if (err_info->validation_bits & ARM_INFO_VALID_PHYSICAL_ADDR) {
			ev->phy_fault_addr = err_info->physical_fault_addr;
			trace_seq_printf(s, " physical fault address: 0x%016llx",
					 (unsigned long long)err_info->physical_fault_addr);
		}
		trace_seq_printf(s, "\n");
		err_info += 1;
	}

	return 0;
}

#ifdef HAVE_CPU_FAULT_ISOLATION
static int is_core_failure(struct ras_arm_err_info *err_info)
{
	if (err_info->validation_bits & ARM_ERR_VALID_FLAGS) {
		/*
		 * core failure:
		 * Bit 0\1\3: (at lease 1)
		 * Bit 2: 0
		 */
		return (err_info->flags & 0xf) && !(err_info->flags & (0x1 << BIT2));
	}
	return 0;
}

static int count_errors(struct ras_arm_event *ev, int sev)
{
	struct ras_arm_err_info *err_info;
	int num_pei;
	int err_info_size = sizeof(struct ras_arm_err_info);
	int num = 0;
	int i;
	int error_count;

	if (ev->pei_len % err_info_size != 0) {
		log(TERM, LOG_ERR,
		    "The event data does not match to the ARM Processor Error Information Structure\n");
		return num;
	}
	num_pei = ev->pei_len / err_info_size;
	err_info = (struct ras_arm_err_info *)(ev->pei_error);

	for (i = 0; i < num_pei; ++i) {
		error_count = 1;
		if (err_info->validation_bits & ARM_ERR_VALID_ERROR_COUNT) {
			/*
			 * The value of this field is defined as follows:
			 * 0: Single Error
			 * 1: Multiple Errors
			 * 2-65535: Error Count
			 */
			error_count = err_info->multiple_error + 1;
		}
		if (sev == GHES_SEV_RECOVERABLE && !is_core_failure(err_info))
			error_count = 0;

		num += error_count;
		err_info += 1;
	}
	log(TERM, LOG_INFO, "%d error in cpu core caught\n", num);
	return num;
}

static int ras_handle_cpu_error(struct trace_seq *s,
				struct tep_record *record,
				struct tep_event *event,
				struct ras_arm_event *ev, time_t now)
{
	unsigned long long val;
	int cpu;
	char *severity;
	struct error_info err_info;

	if (tep_get_field_val(s, event, "cpu", record, &val, 1) < 0)
		return -1;
	cpu = val;
	trace_seq_printf(s, "\n cpu: %d", cpu);

	/* record cpu error */
	if (tep_get_field_val(s, event, "sev", record, &val, 1) < 0)
		return -1;
	/* refer to UEFI_2_9 specification chapter N2.2 Table N-5 */
	switch (val) {
	case GHES_SEV_NO:
		severity = "Informational";
		break;
	case GHES_SEV_CORRECTED:
		severity = "Corrected";
		break;
	case GHES_SEV_RECOVERABLE:
		severity = "Recoverable";
		break;
	default:
	case GHES_SEV_PANIC:
		severity = "Fatal";
	}
	trace_seq_printf(s, "\n severity: %s", severity);

	if (val == GHES_SEV_CORRECTED || val == GHES_SEV_RECOVERABLE) {
		int nums = count_errors(ev, val);

		if (nums > 0) {
			err_info.nums = nums;
			err_info.time = now;
			err_info.err_type = val;
			ras_record_cpu_error(&err_info, cpu);
		}
	}

	return 0;
}
#endif

int ras_arm_event_handler(struct trace_seq *s,
			  struct tep_record *record,
			  struct tep_event *event, void *context)
{
	unsigned long long val;
	struct ras_events *ras = context;
	time_t now;
	struct tm *tm;
	struct ras_arm_event ev;
	int len = 0;

	memset(&ev, 0, sizeof(ev));

	trace_seq_printf(s, "%s ", loglevel_str[LOGLEVEL_ERR]);

	/*
	 * Newer kernels (3.10-rc1 or upper) provide an uptime clock.
	 * On previous kernels, the way to properly generate an event would
	 * be to inject a fake one, measure its timestamp and diff it against
	 * gettimeofday. We won't do it here. Instead, let's use uptime,
	 * falling-back to the event report's time, if "uptime" clock is
	 * not available (legacy kernels).
	 */

	if (ras->use_uptime)
		now = record->ts / user_hz + ras->uptime_diff;
	else
		now = time(NULL);

	tm = localtime(&now);
	if (tm)
		strftime(ev.timestamp, sizeof(ev.timestamp),
			 "%Y-%m-%d %H:%M:%S %z", tm);
	trace_seq_printf(s, "%s", ev.timestamp);

	if (tep_get_field_val(s, event, "affinity", record, &val, 1) < 0)
		return -1;
	ev.affinity = val;
	trace_seq_printf(s, " affinity: %d", ev.affinity);

	if (tep_get_field_val(s, event, "mpidr", record, &val, 1) < 0)
		return -1;
	ev.mpidr = val;
	trace_seq_printf(s, " MPIDR: 0x%llx", (unsigned long long)ev.mpidr);

	if (tep_get_field_val(s, event, "midr", record, &val, 1) < 0)
		return -1;
	ev.midr = val;
	trace_seq_printf(s, " MIDR: 0x%llx", (unsigned long long)ev.midr);

	if (tep_get_field_val(s, event, "running_state", record, &val, 1) < 0)
		return -1;
	ev.running_state = val;
	trace_seq_printf(s, " running_state: %d", ev.running_state);

	if (tep_get_field_val(s, event, "psci_state", record, &val, 1) < 0)
		return -1;
	ev.psci_state = val;
	trace_seq_printf(s, " psci_state: %d", ev.psci_state);

	/* Upstream Kernels up to version 6.10 don't decode UEFI 2.6+ N.17 table */
	if (tep_get_field_val(s, event, "pei_len", record, &val, 0) >= 0) {
		bool legacy_patch = false;

		ev.pei_len = val;
		trace_seq_printf(s, " ARM Processor Err Info data len: %d\n",
				 ev.pei_len);

		ev.pei_error = tep_get_field_raw(s, event, "pei_buf", record, &len, 1);
		if (!ev.pei_error) {
			/* Keep support for OOT CPER N.16/N.17 full table patch */
			ev.pei_error = tep_get_field_raw(s, event, "buf", record, &len, 1);
			if (!ev.pei_error)
				return -1;
			legacy_patch = true;
		}
		display_raw_data(s, ev.pei_error, ev.pei_len);

		parse_arm_processor_err_info(s, &ev);

		if (tep_get_field_val(s, event, "ctx_len", record, &val, 1) < 0)
			return -1;
		ev.ctx_len = val;
		trace_seq_printf(s, " ARM Processor Err Context Info data len: %d\n",
				 ev.ctx_len);

		if (!legacy_patch)
			ev.ctx_error = tep_get_field_raw(s, event, "ctx_buf", record, &len, 1);
		else
			ev.ctx_error = tep_get_field_raw(s, event, "buf1", record, &len, 1);
		if (!ev.ctx_error)
			return -1;
		display_raw_data(s, ev.ctx_error, ev.ctx_len);

		if (tep_get_field_val(s, event, "oem_len", record, &val, 1) < 0)
			return -1;
		ev.oem_len = val;
		trace_seq_printf(s, " Vendor Specific Err Info data len: %d\n",
				 ev.oem_len);

		if (!legacy_patch)
			ev.vsei_error = tep_get_field_raw(s, event, "oem_buf", record, &len, 1);
		else
			ev.vsei_error = tep_get_field_raw(s, event, "buf2", record, &len, 1);
		if (!ev.vsei_error)
			return -1;

#ifdef HAVE_AMP_NS_DECODE
		//decode ampere specific error
		decode_amp_payload0_err_regs(NULL, s,
					     (struct amp_payload0_type_sec *)ev.vsei_error);
#else
		display_raw_data(s, ev.vsei_error, ev.oem_len);
#endif
#ifdef HAVE_CPU_FAULT_ISOLATION
		if (ras_handle_cpu_error(s, record, event, &ev, now) < 0)
			printf("Can't do CPU fault isolation!\n");
#endif
	}

	/* Insert data into the SGBD */
#ifdef HAVE_SQLITE3
	ras_store_arm_record(ras, &ev);
#endif

#ifdef HAVE_ABRT_REPORT
	/* Report event to ABRT */
	ras_report_arm_event(ras, &ev);
#endif

	return 0;
}
