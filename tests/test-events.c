// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2026 Mauro Carvalho Chehab <mchehab+huawei@kernel.org>
 *
 * Test several events.
 */

#include "config.h"

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "core/modules.h"
#include "core/ras-logger.h"
#include "db/ras-db-backend.h"
#include "db/ras-db.h"
#include "events-arch-arm/non-standard-ampere.h"
#include "events-arch-arm/non-standard-nvidia.h"
#include "events-arch-arm/non-standard-yitian.h"
#include "events-arch-arm/ras-arm-handler.h"
#include "events-arch-arm/ras-non-standard-handler.h"
#include "events-arch-riscv/ras-reri-handler.h"
#include "events-arch-x86/ras-erst.h"
#include "events-arch-x86/ras-mce-handler.h"
#include "events/ras-aer-handler.h"
#include "events/ras-cxl-handler.h"
#include "events/ras-devlink-handler.h"
#include "events/ras-diskerror-handler.h"
#include "events/ras-extlog-handler.h"
#include "events/ras-mc-handler.h"
#include "events/ras-memory-failure-handler.h"
#include "events/ras-signal-handler.h"
#include "modules/ras-cpu-isolation.h"
#include "modules/ras-page-isolation.h"
#include "modules/ras-report.h"
#include "modules/unified-sel.h"
#include "tests/trace-mock.h"
#include "tests/unittest.h"

const char *ras_cxl_test_log_type(uint32_t log_type);
void ras_cxl_test_convert_timestamp(unsigned long long timestamp,
				    char *buf, uint16_t size);
const char *ras_cxl_test_uuid(const char *uuid);
const char *ras_diskerror_test_error(int err);
const char *ras_extlog_test_error_type(int type);
const char *ras_extlog_test_severity(int severity);
unsigned long long ras_extlog_test_mask(int lsb);
const char *ras_memory_failure_test_page_type(int page_type);
const char *ras_memory_failure_test_action_result(int result);
int ras_arm_test_parse_processor(struct trace_seq *s,
				 struct ras_arm_event *event);
#ifdef HAVE_CPU_FAULT_ISOLATION
int ras_arm_test_count_errors(struct ras_arm_event *event, int severity);
#endif
size_t ras_ns_test_decoder_count(void);
const char *ras_ns_test_decoder_type(size_t index);
int ras_ns_test_decode(const char *type, struct ras_events *ras,
		       struct trace_seq *seq,
		       struct ras_non_standard_event *event);
const char *ras_reri_test_error_code(uint8_t value);
const char *ras_reri_test_transaction(uint8_t value);
const char *ras_reri_test_address_type(uint8_t value);
const char *ras_reri_test_category(uint8_t value);

#define RUN_FEATURE_GROUP(group_name, test_array) \
	_cmocka_run_group_tests(group_name, test_array, ARRAY_SIZE(test_array), \
				NULL, NULL)
#define RUN_EVENT(group, event, ...) \
	ras_event_test_handler(group, event)(__VA_ARGS__)

#ifdef HAVE_BLK_RQ_ERROR
#define DISKERROR_TRACE_EVENT "block_rq_error"
#else
#define DISKERROR_TRACE_EVENT "block_rq_complete"
#endif

static void init_trace(struct trace_seq *seq, struct tep_record *record,
		       struct ras_events *ras)
{
	memset(record, 0, sizeof(*record));
	memset(ras, 0, sizeof(*ras));
	record->ts = 100;
	ras->use_uptime = true;
	user_hz = 100;
	trace_seq_init(seq);
	trace_mock_start();
}

static const char *finish_trace(struct trace_seq *seq)
{
	trace_seq_terminate(seq);
	trace_mock_stop();
	return seq->buffer ? seq->buffer : "";
}

#ifdef HAVE_NON_STANDARD
static bool decoder_is_registered(const char *type)
{
	for (size_t i = 0; i < ras_ns_test_decoder_count(); i++) {
		const char *candidate = ras_ns_test_decoder_type(i);

		if (candidate && !strcmp(candidate, type))
			return true;
	}
	return false;
}
#endif

static void test_mc_complete_record(void **state)
{
	struct trace_seq seq;
	struct tep_record record;
	struct tep_event event = { 0 };
	struct ras_events ras;
	const char *output;

#ifdef HAVE_MEMORY_CE_PFA
	setenv("PAGE_CE_ACTION", "account", 1);
	setenv("PAGE_CE_THRESHOLD", "50", 1);
	ras_page_account_init();
#endif
	init_trace(&seq, &record, &ras);
	trace_mock_add_value("error_type", HW_EVENT_ERR_CORRECTED);
	trace_mock_add_value("error_count", 2);
	trace_mock_add_raw("msg", "ECC", 4);
	trace_mock_add_raw("label", "DIMM_A0", 8);
	trace_mock_add_value("mc_index", 1);
	trace_mock_add_value("top_layer", 0);
	trace_mock_add_value("middle_layer", 1);
	trace_mock_add_value("lower_layer", 2);
	trace_mock_add_value("address", 0x12345000);
	trace_mock_add_value("grain_bits", 64);
	trace_mock_add_value("syndrome", 0xab);
	trace_mock_add_raw("driver_detail",
			   "APEI location node:0 card:0 module:0 rank:0 device:0 bank:1 row:2",
			   72);
	assert_int_equal(RUN_EVENT("ras", "mc_event", &seq, &record, &event,
				   &ras), 0);
	output = finish_trace(&seq);
	assert_non_null(strstr(output, "2 Corrected errors: ECC on DIMM_A0"));
	assert_non_null(strstr(output, "location: 0:1:2"));
	assert_non_null(strstr(output, "address: 0x12345000"));
	trace_seq_destroy(&seq);
#ifdef HAVE_MEMORY_CE_PFA
	page_record_infos_free();
	unsetenv("PAGE_CE_ACTION");
	unsetenv("PAGE_CE_THRESHOLD");
#endif
}

static void test_mc_missing_field_is_reported(void **state)
{
	struct trace_seq seq;
	struct tep_record record;
	struct tep_event event = { 0 };
	struct ras_events ras;

	ras_logger_clean();
	init_trace(&seq, &record, &ras);
	assert_int_equal(RUN_EVENT("ras", "mc_event", &seq, &record, &event,
				   &ras), 0);
	finish_trace(&seq);
	assert_non_null(strstr(mock_log_buf, "can't parse field #0"));
	ras_logger_clean();
	trace_seq_destroy(&seq);
}

static const struct CMUnitTest mc_tests[] = {
	cmocka_unit_test(test_mc_complete_record),
	cmocka_unit_test(test_mc_missing_field_is_reported),
};

int test_mc(void)
{
	return RUN_FEATURE_GROUP("EDAC memory-controller events", mc_tests);
}

#ifdef HAVE_ABRT_REPORT
static void test_report_formats_mc_event(void **state)
{
	struct ras_mc_event event = {
		.error_count = 2,
		.error_type = "Corrected",
		.msg = "ECC",
		.label = "DIMM0",
		.mc_index = 1,
		.top_layer = 0,
		.middle_layer = 1,
		.lower_layer = 2,
		.address = 0x1000,
		.grain = 64,
		.syndrome = 7,
		.driver_detail = "detail",
	};
	char output[2048];

	strscpy(event.timestamp, "2026-08-25 00:00:00 +0000",
		sizeof(event.timestamp));
	assert_int_equal(ras_report_test_format(MC_EVENT, &event, output,
						sizeof(output)), 0);
	assert_non_null(strstr(output, "BACKTRACE=timestamp=2026-08-25"));
	assert_non_null(strstr(output, "error_type=Corrected"));
	assert_non_null(strstr(output, "address=4096"));
	assert_int_equal(ras_report_test_format(NR_EVENTS, &event, output,
						sizeof(output)), -1);
	assert_int_equal(ras_report_test_format(MC_EVENT, NULL, output,
						sizeof(output)), -1);
}

static const struct CMUnitTest report_tests[] = {
	cmocka_unit_test(test_report_formats_mc_event),
};

int test_report(void)
{
	return RUN_FEATURE_GROUP("ABRT report formatting", report_tests);
}
#endif

#ifdef HAVE_AER
static void test_aer_corrected_record(void **state)
{
	struct trace_seq seq;
	struct tep_record record;
	struct tep_event event = { 0 };
	struct ras_events ras;
	const char *output;

	init_trace(&seq, &record, &ras);
	trace_mock_add_value("severity", HW_EVENT_AER_CORRECTED);
	trace_mock_add_raw("dev_name", "not-a-bdf", 10);
	trace_mock_add_value("status", BIT_ULL(0) | BIT_ULL(6));
	trace_mock_add_value("tlp_header_valid", 0);
	assert_int_equal(RUN_EVENT("ras", "aer_event", &seq, &record, &event,
				   &ras), 0);
	output = finish_trace(&seq);
	assert_non_null(strstr(output, "Receiver Error, Bad TLP"));
	assert_non_null(strstr(output, "Corrected"));
	trace_seq_destroy(&seq);
}

static void test_aer_requires_severity(void **state)
{
	struct trace_seq seq;
	struct tep_record record;
	struct tep_event event = { 0 };
	struct ras_events ras;

	init_trace(&seq, &record, &ras);
	assert_int_equal(RUN_EVENT("ras", "aer_event", &seq, &record, &event,
				   &ras), -1);
	finish_trace(&seq);
	trace_seq_destroy(&seq);
}

static const struct CMUnitTest aer_tests[] = {
	cmocka_unit_test(test_aer_corrected_record),
	cmocka_unit_test(test_aer_requires_severity),
};

int test_aer(void)
{
	return RUN_FEATURE_GROUP("PCIe AER", aer_tests);
}
#endif

#ifdef HAVE_ARM
static void test_arm_processor_payload(void **state)
{
	struct ras_arm_err_info info = {
		.validation_bits = BIT(0) | BIT(1) | BIT(2) | BIT(3) | BIT(4),
		.type = BIT(1),
		.multiple_error = 2,
		.flags = BIT(0),
		.error_info = BIT(0) | (1ULL << 16),
		.virt_fault_addr = 0x1000,
		.physical_fault_addr = 0x2000,
	};
	struct ras_arm_event event = {
		.pei_error = (const uint8_t *)&info,
		.pei_len = sizeof(info),
	};
	struct trace_seq seq;

	trace_seq_init(&seq);
	assert_int_equal(ras_arm_test_parse_processor(&seq, &event), 0);
	trace_seq_terminate(&seq);
	assert_int_equal(event.error_count, 3);
	assert_non_null(strstr(event.error_types, "cache error"));
	assert_non_null(strstr(seq.buffer, "virtual fault address"));
#ifdef HAVE_CPU_FAULT_ISOLATION
	assert_int_equal(ras_arm_test_count_errors(&event, GHES_SEV_CORRECTED), 3);
#endif
	event.pei_len--;
	assert_int_equal(ras_arm_test_parse_processor(&seq, &event), -1);
	trace_seq_destroy(&seq);
}

static const struct CMUnitTest arm_tests[] = {
	cmocka_unit_test(test_arm_processor_payload),
};

int test_arm(void)
{
	return RUN_FEATURE_GROUP("ARM processor events", arm_tests);
}
#endif

#ifdef HAVE_CPU_FAULT_ISOLATION
static void test_cpu_isolation_configuration(void **state)
{
	unsigned long value;

	assert_int_equal(ras_cpu_isolation_test_parse("18", false, &value), 0);
	assert_int_equal(value, 18);
	assert_int_equal(ras_cpu_isolation_test_parse("2h", true, &value), 0);
	assert_int_equal(value, 7200);
	assert_int_equal(ras_cpu_isolation_test_parse("5x", true, &value), -1);
	assert_int_equal(ras_cpu_isolation_test_parse("", false, &value), -1);
}

static const struct CMUnitTest cpu_isolation_tests[] = {
	cmocka_unit_test(test_cpu_isolation_configuration),
};

int test_cpu_isolation(void)
{
	return RUN_FEATURE_GROUP("CPU fault isolation", cpu_isolation_tests);
}
#endif

#ifdef HAVE_CXL
static void test_cxl_common_decoders(void **state)
{
	const unsigned char uuid[16] = {
		0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
		0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff,
	};
	char timestamp[64];

	assert_string_equal(ras_cxl_test_log_type(0), "Informational");
	assert_string_equal(ras_cxl_test_log_type(3), "Fatal");
	assert_string_equal(ras_cxl_test_log_type(99), "Unknown");
	assert_string_equal(ras_cxl_test_uuid((const char *)uuid),
			    "00112233-4455-6677-8899-aabbccddeeff");
	ras_cxl_test_convert_timestamp(0, timestamp, sizeof(timestamp));
	assert_string_equal(timestamp, "1970-01-01 00:00:00 +0000");
}

static void test_cxl_poison_record(void **state)
{
	struct trace_seq seq;
	struct tep_record record;
	struct tep_event event = { 0 };
	struct ras_events ras;
	const char *output;

	init_trace(&seq, &record, &ras);
	trace_mock_add_raw("memdev", "mem0", 5);
	trace_mock_add_raw("host", "host0", 6);
	trace_mock_add_value("serial", 0x1234);
	trace_mock_add_value("trace_type", 1);
	trace_mock_add_raw("region", "region0", 8);
	trace_mock_add_raw("uuid", "uuid0", 6);
	trace_mock_add_value("hpa", 0x1000);
	trace_mock_add_value("hpa_alias0", 0x2000);
	trace_mock_add_value("dpa", 0x3000);
	trace_mock_add_value("dpa_length", 0x40);
	trace_mock_add_value("source", 3);
	trace_mock_add_value("flags", 0);
	assert_int_equal(RUN_EVENT("cxl", "cxl_poison", &seq, &record,
				   &event, &ras), 0);
	output = finish_trace(&seq);
	assert_non_null(strstr(output, "trace_type:Inject"));
	assert_non_null(strstr(output, "source:Injected"));
	assert_non_null(strstr(output, "dpa:0x3000"));
	trace_seq_destroy(&seq);
}

static void test_all_cxl_handlers_reject_empty_record(void **state)
{
	static const char * const events[] = {
		"cxl_poison",
		"cxl_aer_uncorrectable_error",
		"cxl_aer_correctable_error",
		"cxl_overflow",
		"cxl_generic_event",
		"cxl_general_media",
		"cxl_dram",
		"cxl_memory_module",
		"cxl_memory_sparing",
	};

	for (size_t i = 0; i < ARRAY_SIZE(events); i++) {
		struct trace_seq seq;
		struct tep_record record;
		struct tep_event event = { 0 };
		struct ras_events ras;

		init_trace(&seq, &record, &ras);
		assert_int_equal(RUN_EVENT("cxl", events[i], &seq, &record,
					   &event, &ras), -1);
		finish_trace(&seq);
		trace_seq_destroy(&seq);
	}
}

static const struct CMUnitTest cxl_tests[] = {
	cmocka_unit_test(test_cxl_common_decoders),
	cmocka_unit_test(test_cxl_poison_record),
	cmocka_unit_test(test_all_cxl_handlers_reject_empty_record),
};

int test_cxl(void)
{
	return RUN_FEATURE_GROUP("CXL events", cxl_tests);
}
#endif

#ifdef HAVE_DEVLINK
static void test_devlink_health_record(void **state)
{
	struct trace_seq seq;
	struct tep_record record;
	struct tep_event event = { 0 };
	struct ras_events ras;
	const char *output;

	init_trace(&seq, &record, &ras);
	trace_mock_add_raw("bus_name", "pci", 4);
	trace_mock_add_raw("dev_name", "0000:01:00.0", 13);
	trace_mock_add_raw("driver_name", "driver", 7);
	trace_mock_add_raw("reporter_name", "fw", 3);
	trace_mock_add_raw("msg", "health failure", 15);
	assert_int_equal(RUN_EVENT("devlink", "devlink_health_report", &seq,
				   &record, &event, &ras), 0);
	output = finish_trace(&seq);
	assert_non_null(strstr(output, "1970"));
	trace_seq_destroy(&seq);
}

static void test_devlink_filter_and_net_timeout(void **state)
{
	struct trace_seq seq;
	struct tep_record record;
	struct tep_event event = { 0 };
	struct ras_events ras;

	init_trace(&seq, &record, &ras);
	ras.filters[DEVLINK_EVENT] = (void *)1;
	trace_mock_set_filter_result(FILTER_MATCH);
	assert_int_equal(RUN_EVENT("devlink", "devlink_health_report", &seq,
				   &record, &event, &ras), 0);
	finish_trace(&seq);
	trace_seq_destroy(&seq);

	init_trace(&seq, &record, &ras);
	trace_mock_add_raw("name", "eth0", 5);
	trace_mock_add_raw("driver", "netdrv", 7);
	trace_mock_add_value("queue_index", 3);
	assert_int_equal(RUN_EVENT("net", "net_dev_xmit_timeout", &seq,
				   &record, &event, &ras), 0);
	finish_trace(&seq);
	trace_seq_destroy(&seq);
}

static const struct CMUnitTest devlink_tests[] = {
	cmocka_unit_test(test_devlink_health_record),
	cmocka_unit_test(test_devlink_filter_and_net_timeout),
};

int test_devlink(void)
{
	return RUN_FEATURE_GROUP("devlink health", devlink_tests);
}
#endif

#ifdef HAVE_DISKERROR
static void test_diskerror_mapping_and_record(void **state)
{
	struct trace_seq seq;
	struct tep_record record;
	struct tep_event event = { 0 };
	struct ras_events ras;
	const char *output;

	assert_string_equal(ras_diskerror_test_error(-EIO), "I/O error");
	assert_string_equal(ras_diskerror_test_error(-12345),
			    "unknown block error");
	init_trace(&seq, &record, &ras);
	trace_mock_add_value("dev", MKDEV(8, 1));
	trace_mock_add_value("sector", 4096);
	trace_mock_add_value("nr_sector", 8);
	trace_mock_add_value("error", (unsigned long long)-EIO);
	trace_mock_add_raw("rwbs", "R", 2);
	trace_mock_add_raw("cmd", "read", 5);
	assert_int_equal(RUN_EVENT("block", DISKERROR_TRACE_EVENT, &seq,
				   &record, &event, &ras), 0);
	output = finish_trace(&seq);
	assert_non_null(strstr(output, "[ERROR]"));
	trace_seq_destroy(&seq);
}

static const struct CMUnitTest diskerror_tests[] = {
	cmocka_unit_test(test_diskerror_mapping_and_record),
};

int test_diskerror(void)
{
	return RUN_FEATURE_GROUP("disk I/O errors", diskerror_tests);
}
#endif

#ifdef HAVE_EXTLOG
static void test_extlog_mappings(void **state)
{
	assert_string_equal(ras_extlog_test_error_type(2), "single-bit ECC");
	assert_string_equal(ras_extlog_test_error_type(99), "unknown-type");
	assert_string_equal(ras_extlog_test_severity(2), "corrected");
	assert_string_equal(ras_extlog_test_severity(99), "unknown-severity");
	assert_int_equal(ras_extlog_test_mask(0), ~0ULL);
	assert_int_equal(ras_extlog_test_mask(12), ~((1ULL << 12) - 1));
	assert_int_equal(ras_extlog_test_mask(0xff), ~0ULL);
}

static const struct CMUnitTest extlog_tests[] = {
	cmocka_unit_test(test_extlog_mappings),
};

int test_extlog(void)
{
	return RUN_FEATURE_GROUP("ACPI EXTLOG", extlog_tests);
}
#endif

#ifdef HAVE_MEMORY_FAILURE
static void test_memory_failure_record(void **state)
{
	struct trace_seq seq;
	struct tep_record record;
	struct tep_event event = { 0 };
	struct ras_events ras;
	const char *output;

	assert_string_equal(ras_memory_failure_test_page_type(0),
			    "reserved kernel page");
	assert_string_equal(ras_memory_failure_test_page_type(255),
			    "unknown page");
	assert_string_equal(ras_memory_failure_test_action_result(3),
			    "Recovered");
	assert_string_equal(ras_memory_failure_test_action_result(99), "unknown");
	init_trace(&seq, &record, &ras);
	trace_mock_add_value("pfn", 0x123);
	trace_mock_add_value("type", 4);
	trace_mock_add_value("result", 3);
	assert_int_equal(RUN_EVENT("ras", "memory_failure_event", &seq,
				   &record, &event, &ras), 0);
	output = finish_trace(&seq);
	assert_non_null(strstr(output, "pfn=0x123"));
	assert_non_null(strstr(output, "page_type=huge page"));
	assert_non_null(strstr(output, "action_result=Recovered"));
	trace_seq_destroy(&seq);
}

static const struct CMUnitTest memory_failure_tests[] = {
	cmocka_unit_test(test_memory_failure_record),
};

int test_memory_failure(void)
{
	return RUN_FEATURE_GROUP("memory failure", memory_failure_tests);
}
#endif

#ifdef HAVE_MEMORY_CE_PFA
static void test_page_pfa_configuration_and_accounting(void **state)
{
	unsigned long value;

	unsetenv("PAGE_CE_THRESHOLD");
	assert_int_equal(ras_page_isolation_test_parse_value("50", false,
							     &value), 0);
	assert_int_equal(value, 50);
	setenv("PAGE_CE_ACTION", "account", 1);
	setenv("PAGE_CE_THRESHOLD", "2", 1);
	setenv("PAGE_CE_REFRESH_CYCLE", "10s", 1);
	ras_page_account_init();
	ras_record_page_error(0x1234, 1, 10);
	ras_record_page_error(0x1fff, 1, 11);
	page_record_infos_free();
	unsetenv("PAGE_CE_ACTION");
	unsetenv("PAGE_CE_THRESHOLD");
	unsetenv("PAGE_CE_REFRESH_CYCLE");
}

static const struct CMUnitTest memory_ce_pfa_tests[] = {
	cmocka_unit_test(test_page_pfa_configuration_and_accounting),
};

int test_memory_ce_pfa(void)
{
	return RUN_FEATURE_GROUP("memory page CE PFA", memory_ce_pfa_tests);
}
#endif

#ifdef HAVE_MEMORY_ROW_CE_PFA
static void test_row_pfa_parser_and_accounting(void **state)
{
	static const char detail[] =
		"APEI location node:0 card:1 module:2 rank:3 device:4 bank:5 row:6";
	struct row_record record;
	unsigned long value;

	assert_int_equal(ras_page_isolation_test_parse_row(detail, &record), 0);
	assert_int_equal(record.type, GHES);
	assert_int_equal(record.location_fields[APEI_ROW], 6);
	assert_int_not_equal(ras_page_isolation_test_parse_row("incomplete",
								&record), 0);
	setenv("ROW_CE_THRESHOLD", "2k", 1);
	assert_int_equal(ras_page_isolation_test_parse_value("2k", true,
							     &value), 0);
	assert_int_equal(value, 2000);
	setenv("ROW_CE_ACTION", "account", 1);
	setenv("ROW_CE_THRESHOLD", "2", 1);
	setenv("ROW_CE_REFRESH_CYCLE", "10s", 1);
	ras_row_account_init();
	ras_record_row_error(detail, 1, 10, 0x1000);
	ras_record_row_error(detail, 1, 11, 0x2000);
	row_record_infos_free();
	unsetenv("ROW_CE_ACTION");
	unsetenv("ROW_CE_THRESHOLD");
	unsetenv("ROW_CE_REFRESH_CYCLE");
}

static const struct CMUnitTest memory_row_ce_pfa_tests[] = {
	cmocka_unit_test(test_row_pfa_parser_and_accounting),
};

int test_memory_row_ce_pfa(void)
{
	return RUN_FEATURE_GROUP("memory row CE PFA", memory_row_ce_pfa_tests);
}
#endif

#ifdef HAVE_SIGNAL
static void test_signal_record(void **state)
{
	struct trace_seq seq;
	struct tep_record record;
	struct tep_event event = { 0 };
	struct ras_events ras;
	const char *output;

	init_trace(&seq, &record, &ras);
	trace_mock_add_value("sig", SIGBUS);
	trace_mock_add_value("errno", 0);
	trace_mock_add_value("code", BUS_ADRERR);
	trace_mock_add_raw("comm", "worker", 7);
	trace_mock_add_value("pid", 42);
	trace_mock_add_value("group", 1);
	trace_mock_add_value("result", 0);
	assert_int_equal(RUN_EVENT("signal", "signal_generate", &seq, &record,
				   &event, &ras), 0);
	output = finish_trace(&seq);
	assert_non_null(strstr(output, "BUS_ADRERR"));
	assert_non_null(strstr(output, "Delivered"));
	assert_non_null(strstr(output, "non-existent address"));
	trace_seq_destroy(&seq);

	init_trace(&seq, &record, &ras);
	trace_mock_add_value("sig", SIGBUS);
	trace_mock_add_value("errno", 0);
	trace_mock_add_value("code", 99);
	trace_mock_add_raw("comm", "worker", 7);
	trace_mock_add_value("pid", 42);
	trace_mock_add_value("group", 1);
	trace_mock_add_value("result", 99);
	assert_int_equal(RUN_EVENT("signal", "signal_generate", &seq, &record,
				   &event, &ras), 0);
	output = finish_trace(&seq);
	assert_non_null(strstr(output, "code: Unknown"));
	assert_non_null(strstr(output, "res: Unknown"));
	assert_non_null(strstr(output, "msg: Unknown"));
	trace_seq_destroy(&seq);
}

static const struct CMUnitTest signal_tests[] = {
	cmocka_unit_test(test_signal_record),
};

int test_signal(void)
{
	return RUN_FEATURE_GROUP("signal events", signal_tests);
}
#endif

#ifdef HAVE_RERI
static void test_reri_mappings_and_record(void **state)
{
	struct trace_seq seq;
	struct tep_record record;
	struct tep_event event = { 0 };
	struct ras_events ras;
	const char *output;
	uint64_t status = BIT_ULL(0) |
		((uint64_t)RERI_TT_EXPLICIT_READ << 8) |
		((uint64_t)RERI_AIT_SPA << 12) |
		((uint64_t)RERI_EC_CBA << 24);

	assert_string_equal(ras_reri_test_error_code(RERI_EC_CBA),
			    "Cache block data error");
	assert_string_equal(ras_reri_test_error_code(200), "Unknown error code");
	assert_string_equal(ras_reri_test_transaction(RERI_TT_EXPLICIT_READ),
			    "Explicit Read");
	assert_string_equal(ras_reri_test_address_type(RERI_AIT_SPA),
			    "Supervisor Physical Address");
	assert_string_equal(ras_reri_test_category(RERI_EC_CBA), "Cache");
	init_trace(&seq, &record, &ras);
	trace_mock_add_value("err_src_id", 7);
	trace_mock_add_value("source_type", RERI_SOURCE_TYPE_IOMMU);
	trace_mock_add_value("status", status);
	trace_mock_add_value("addr_info", 0xfeed0000);
	assert_int_equal(RUN_EVENT("ras", "reri_event", &seq, &record, &event,
				   &ras), 0);
	output = finish_trace(&seq);
	assert_non_null(strstr(output, "source: IOMMU"));
	assert_non_null(strstr(output, "error_type: Cache"));
	assert_non_null(strstr(output, "address: 0xfeed0000"));
	trace_seq_destroy(&seq);

	init_trace(&seq, &record, &ras);
	trace_mock_add_value("err_src_id", UINT16_MAX + 1ULL);
	assert_int_equal(RUN_EVENT("ras", "reri_event", &seq, &record, &event,
				   &ras), -1);
	finish_trace(&seq);
	trace_seq_destroy(&seq);
}

static const struct CMUnitTest reri_tests[] = {
	cmocka_unit_test(test_reri_mappings_and_record),
};

int test_reri(void)
{
	return RUN_FEATURE_GROUP("RISC-V RERI", reri_tests);
}
#endif

#ifdef HAVE_OPENBMC_UNIFIED_SEL
static void test_openbmc_sel_commands(void **state)
{
	system_mock_start(0);
	assert_int_equal(openbmc_unified_sel_log(HW_EVENT_AER_CORRECTED,
						 "0000:02:03.1", BIT_ULL(0)), 0);
	assert_int_equal(system_mock_call_count(), 1);
	assert_non_null(strstr(system_mock_last_command(), "0x19 0x02"));
	assert_non_null(strstr(system_mock_last_command(), "0x00"));
	assert_int_equal(openbmc_unified_sel_log(HW_EVENT_AER_CORRECTED,
						 "invalid", 1), -1);
	system_mock_start(1);
	assert_int_equal(openbmc_unified_sel_log(HW_EVENT_AER_CORRECTED,
						 "0000:02:03.1", 1), -1);
	system_mock_stop();
}

static const struct CMUnitTest openbmc_tests[] = {
	cmocka_unit_test(test_openbmc_sel_commands),
};

int test_openbmc_sel(void)
{
	return RUN_FEATURE_GROUP("OpenBMC unified SEL", openbmc_tests);
}
#endif

#ifdef HAVE_MCE
static void test_mce_vendor_decoders(void **state)
{
	struct mce_event event = {
		.status = MCI_STATUS_VAL,
	};
	struct mce_priv priv = { .cputype = CPU_ZHAOXIN };
	struct ras_events ras = { .mce_priv = &priv };

	decode_amd_errcode(&event);
	assert_string_equal(event.error_msg,
			    "Corrected error, no action required.");
	memset(&event, 0, sizeof(event));
	event.status = MCI_STATUS_VAL;
	assert_int_equal(parse_zhaoxin_event(&ras, &event), 0);
	assert_non_null(strstr(event.error_msg, "Corrected Error"));

	memset(&event, 0, sizeof(event));
	event.status = MCI_STATUS_VAL;
	event.bank = MCE_EXTENDED_BANK;
	priv.cputype = CPU_GENERIC;
	assert_int_equal(parse_intel_event(&ras, &event), 0);
	assert_string_equal(event.bank_name, "THERMAL EVENT");
}

static const struct CMUnitTest mce_tests[] = {
	cmocka_unit_test(test_mce_vendor_decoders),
};

int test_mce(void)
{
	return RUN_FEATURE_GROUP("machine-check decoding", mce_tests);
}
#endif

#ifdef HAVE_ERST
struct test_erst_record {
	uint64_t status, misc, addr, mcgstatus, ip, tsc, time;
	uint8_t cpuvendor, inject_flags, severity, pad;
	uint32_t cpuid;
	uint8_t cs, bank, cpu, finished;
	uint32_t extcpu, socketid, apicid;
	uint64_t mcgcap, synd, ipid, ppin;
	uint32_t microcode;
};

static void test_erst_record_reader(void **state)
{
	char path[] = "/tmp/rasdaemon-erst-XXXXXX";
	struct test_erst_record input = {
		.status = 0x11,
		.addr = 0x22,
		.extcpu = 3,
		.bank = 4,
		.ipid = 0x55,
	};
	struct mce_event event = { 0 };
	int fd = mkstemp(path);

	assert_true(fd >= 0);
	assert_int_equal(write(fd, &input, sizeof(input)), sizeof(input));
	assert_int_equal(close(fd), 0);
	unsetenv(ERST_DELETE);
	assert_int_equal(ras_erst_test_read(path, &event), 0);
	assert_int_equal(event.status, input.status);
	assert_int_equal(event.addr, input.addr);
	assert_int_equal(event.cpu, input.extcpu);
	assert_int_equal(event.bank, input.bank);
	assert_int_equal(event.ipid, input.ipid);
	assert_int_equal(truncate(path, 4), 0);
	assert_int_equal(ras_erst_test_read(path, &event), -1);
	assert_int_equal(unlink(path), 0);
	assert_int_equal(ras_erst_test_read(path, &event), -1);
}

static const struct CMUnitTest erst_tests[] = {
	cmocka_unit_test(test_erst_record_reader),
};

int test_erst(void)
{
	return RUN_FEATURE_GROUP("ERST records", erst_tests);
}
#endif

#ifdef HAVE_NON_STANDARD
static void assert_vendor_descriptor_count(struct db_table_descriptor_list list,
					   size_t expected)
{
	assert_int_equal(list.num_tables, expected);
	for (size_t i = 0; i < list.num_tables; i++) {
		assert_non_null(list.tables[i]);
		assert_non_null(list.tables[i]->name);
		assert_true(list.tables[i]->num_fields > 1);
	}
}
#endif

#ifdef HAVE_DB
static void test_db_table_registry(void **state)
{
	static const struct db_table_descriptor descriptor = {
		.name = "test-table",
	};
	struct db_desc_and_stmt entry = {
		.desc = &descriptor,
	};
	struct ras_module_ctx ctx = { 0 };

	assert_int_equal(ras_db_table_register(NULL, &entry), -EINVAL);
	assert_int_equal(ras_db_table_register(&ctx, NULL), -EINVAL);
	assert_int_equal(ras_db_table_register(&ctx, &entry), 0);
	assert_int_equal(ras_db_table_register(&ctx, &entry), -EEXIST);
	ras_db_table_unregister(&ctx);
	assert_int_equal(ras_db_table_register(&ctx, &entry), 0);
	ras_db_table_unregister(&ctx);
}

static void test_database_registry_and_environment(void **state)
{
	static const struct ras_db_backend_ops incomplete_ops = { 0 };
	struct ras_db_backend_entry incomplete = {
		.name = "incomplete",
		.ops = &incomplete_ops,
	};
	const char *available;

	assert_int_equal(db_backend_register(NULL), -EINVAL);
	assert_int_equal(db_backend_register(&incomplete), -EINVAL);
	assert_false(db_backend_is_registered(NULL));
	assert_false(db_backend_is_registered("incomplete"));
	available = db_list_available_backends();
	assert_non_null(available);
#ifdef HAVE_SQLITE3
	assert_non_null(strstr(available, "sqlite3"));
	assert_true(db_backend_is_registered("sqlite3"));
	assert_int_equal(db_backend_enable("sqlite3"), 0);
#endif
	assert_int_equal(db_backend_enable("does-not-exist"), -1);
	unsetenv("RAS_TEST_DB_BOOL");
	assert_int_equal(env_or_bool("RAS_TEST_DB_BOOL", 1), 1);
	setenv("RAS_TEST_DB_BOOL", "no", 1);
	assert_false(env_or_bool("RAS_TEST_DB_BOOL", 1));
	setenv("RAS_TEST_DB_BOOL", "yes", 1);
	assert_true(env_or_bool("RAS_TEST_DB_BOOL", 0));
	setenv("RAS_TEST_DB_INT", "42", 1);
	assert_int_equal(env_or_int("RAS_TEST_DB_INT", 7), 42);
	setenv("RAS_TEST_DB_INT", "invalid", 1);
	assert_int_equal(env_or_int("RAS_TEST_DB_INT", 7), 7);
	unsetenv("RAS_TEST_DB_BOOL");
	unsetenv("RAS_TEST_DB_INT");
}

static const struct CMUnitTest database_tests[] = {
	cmocka_unit_test(test_database_registry_and_environment),
	cmocka_unit_test(test_db_table_registry),
};

int test_database(void)
{
	return RUN_FEATURE_GROUP("database registry", database_tests);
}
#endif

#ifdef HAVE_AMP_NS_DECODE
static void test_ampere_decoder_registration(void **state)
{
	struct amp_payload0_type_sec payload = { .type = AMP_RAS_TYPE_CPU };
	struct ras_non_standard_event event = {
		.timestamp = "2026-08-25 00:00:00 +0000",
		.error = (const uint8_t *)&payload,
		.length = sizeof(payload),
	};
	struct ras_events ras = { 0 };
	struct trace_seq seq;

	assert_true(decoder_is_registered("e8ed898d-df16-43cc-8ecc-54f060ef157f"));
#ifdef HAVE_DB
	assert_vendor_descriptor_count(ampere_table_descriptors(), 4);
#endif
	trace_seq_init(&seq);
	assert_int_equal(ras_ns_test_decode(
		"e8ed898d-df16-43cc-8ecc-54f060ef157f",
		&ras, &seq, &event), 0);
	trace_seq_destroy(&seq);
}

static const struct CMUnitTest amp_ns_tests[] = {
	cmocka_unit_test(test_ampere_decoder_registration),
};

int test_amp_ns(void)
{
	return RUN_FEATURE_GROUP("Ampere non-standard CPER", amp_ns_tests);
}
#endif

#ifdef HAVE_HISI_NS_DECODE
static void test_hisilicon_decoder_registration(void **state)
{
	assert_true(decoder_is_registered("c8b328a8-9917-4af6-9a13-2e08ab2e7586"));
	assert_true(decoder_is_registered("1f8161e1-55d6-41e6-bd10-7afd1dc5f7c5"));
	assert_true(decoder_is_registered("45534ea6-ce23-4115-8535-e07ab3aef91d"));
	assert_true(decoder_is_registered("b2889fc9-e7d7-4f9d-a867-af42e98be772"));
#ifdef HAVE_DB
	assert_vendor_descriptor_count(hisilicon_table_descriptors(), 1);
	assert_vendor_descriptor_count(hip08_table_descriptors(), 3);
#endif
}

static const struct CMUnitTest hisi_ns_tests[] = {
	cmocka_unit_test(test_hisilicon_decoder_registration),
};

int test_hisi_ns(void)
{
	return RUN_FEATURE_GROUP("HiSilicon non-standard CPER", hisi_ns_tests);
}
#endif

#ifdef HAVE_JAGUAR_NS_DECODE
static void test_jaguarmicro_decoder_registration(void **state)
{
	assert_true(decoder_is_registered("82d78ba3-fa14-407a-ba0e-f3ba8170013c"));
	assert_true(decoder_is_registered("f9723053-2558-49b1-b58a-1c1a82492a62"));
	assert_true(decoder_is_registered("2d31de54-3037-4f24-a283-f69ca1ec0b9a"));
	assert_true(decoder_is_registered("dac80d69-0a72-4eba-8114-148ee344af06"));
	assert_true(decoder_is_registered("746f06fe-405e-451f-8d09-02e802ed984a"));
#ifdef HAVE_DB
	assert_vendor_descriptor_count(jaguarmicro_table_descriptors(), 1);
#endif
}

static const struct CMUnitTest jaguar_ns_tests[] = {
	cmocka_unit_test(test_jaguarmicro_decoder_registration),
};

int test_jaguar_ns(void)
{
	return RUN_FEATURE_GROUP("JaguarMicro non-standard CPER", jaguar_ns_tests);
}
#endif

#ifdef HAVE_NVIDIA_NS_DECODE
static void test_nvidia_decoder(void **state)
{
	struct nvidia_cper_sec payload = {
		.signature = "NVIDIA",
		.error_type = 1,
		.number_regs = 0,
	};
	struct trace_seq seq;

	assert_true(decoder_is_registered(NVIDIA_GRACE_SEC_TYPE_UUID));
	assert_true(decoder_is_registered(NVIDIA_VERA_SEC_TYPE_UUID));
#ifdef HAVE_DB
	assert_vendor_descriptor_count(nvidia_table_descriptors(), 2);
#endif
	trace_seq_init(&seq);
	decode_nvidia_cper_sec(NULL, &seq, &payload, sizeof(payload));
	trace_seq_terminate(&seq);
	assert_non_null(strstr(seq.buffer, "NVIDIA"));
	trace_seq_destroy(&seq);
}

static const struct CMUnitTest nvidia_ns_tests[] = {
	cmocka_unit_test(test_nvidia_decoder),
};

int test_nvidia_ns(void)
{
	return RUN_FEATURE_GROUP("NVIDIA non-standard CPER", nvidia_ns_tests);
}
#endif

#ifdef HAVE_YITIAN_NS_DECODE
static void test_yitian_decoder_registration(void **state)
{
	struct yitian_ddr_payload_type_sec payload = {
		.header.type = YITIAN_RAS_TYPE_DDR,
	};
	struct ras_non_standard_event event = {
		.timestamp = "2026-08-25 00:00:00 +0000",
		.error = (const uint8_t *)&payload,
		.length = sizeof(payload),
	};
	struct ras_events ras = { 0 };
	struct trace_seq seq;
	const char *type = "a6980811-16ea-4e4d-b936-fb00a23ff29c";

	assert_true(decoder_is_registered(type));
#ifdef HAVE_DB
	assert_vendor_descriptor_count(yitian_table_descriptors(), 1);
#endif
	trace_seq_init(&seq);
	assert_int_equal(ras_ns_test_decode(type, &ras, &seq, &event), 0);
	trace_seq_destroy(&seq);
}

static const struct CMUnitTest yitian_ns_tests[] = {
	cmocka_unit_test(test_yitian_decoder_registration),
};

int test_yitian_ns(void)
{
	return RUN_FEATURE_GROUP("Yitian non-standard CPER", yitian_ns_tests);
}
#endif

#ifdef HAVE_ABRT_REPORT
REGISTER_TEST(TEST_GROUP_ACTIONS, test_report, 0);
#endif
#ifdef HAVE_CPU_FAULT_ISOLATION
REGISTER_TEST(TEST_GROUP_ACTIONS, test_cpu_isolation, 0);
#endif
#ifdef HAVE_MEMORY_CE_PFA
REGISTER_TEST(TEST_GROUP_ACTIONS, test_memory_ce_pfa, 0);
#endif
#ifdef HAVE_MEMORY_ROW_CE_PFA
REGISTER_TEST(TEST_GROUP_ACTIONS, test_memory_row_ce_pfa, 0);
#endif
#ifdef HAVE_OPENBMC_UNIFIED_SEL
REGISTER_TEST(TEST_GROUP_ACTIONS, test_openbmc_sel, 0);
#endif
#ifdef HAVE_ERST
REGISTER_TEST(TEST_GROUP_EVENTS, test_erst, 0);
#endif
#ifdef HAVE_DB
REGISTER_TEST(TEST_GROUP_DATABASE, test_database, 0);
#endif
#ifdef HAVE_AMP_NS_DECODE
REGISTER_TEST(TEST_GROUP_ARM_EVENTS, test_amp_ns, 0);
#endif
#ifdef HAVE_HISI_NS_DECODE
REGISTER_TEST(TEST_GROUP_ARM_EVENTS, test_hisi_ns, 0);
#endif
#ifdef HAVE_JAGUAR_NS_DECODE
REGISTER_TEST(TEST_GROUP_ARM_EVENTS, test_jaguar_ns, 0);
#endif
#ifdef HAVE_NVIDIA_NS_DECODE
REGISTER_TEST(TEST_GROUP_ARM_EVENTS, test_nvidia_ns, 0);
#endif
#ifdef HAVE_YITIAN_NS_DECODE
REGISTER_TEST(TEST_GROUP_ARM_EVENTS, test_yitian_ns, 0);
#endif
