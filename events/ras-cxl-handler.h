/* SPDX-License-Identifier: GPL-2.0-or-later */

/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023. All rights reserved.
 */

#ifndef __RAS_CXL_HANDLER_H
#define __RAS_CXL_HANDLER_H

#include <traceevent/event-parse.h>

#include "core/ras-events.h"
#include "core/types.h"

int ras_cxl_poison_event_handler(struct trace_seq *s,
				 struct tep_record *record,
				 struct tep_event *event, void *context);

int ras_cxl_aer_ue_event_handler(struct trace_seq *s,
				 struct tep_record *record,
				 struct tep_event *event, void *context);

int ras_cxl_aer_ce_event_handler(struct trace_seq *s,
				 struct tep_record *record,
				 struct tep_event *event, void *context);
int ras_cxl_overflow_event_handler(struct trace_seq *s,
				   struct tep_record *record,
				   struct tep_event *event, void *context);
int ras_cxl_generic_event_handler(struct trace_seq *s,
				  struct tep_record *record,
				  struct tep_event *event, void *context);
int ras_cxl_general_media_event_handler(struct trace_seq *s,
					struct tep_record *record,
					struct tep_event *event, void *context);
int ras_cxl_dram_event_handler(struct trace_seq *s,
			       struct tep_record *record,
			       struct tep_event *event, void *context);
int ras_cxl_memory_module_event_handler(struct trace_seq *s,
					struct tep_record *record,
					struct tep_event *event, void *context);
int ras_cxl_memory_sparing_event_handler(struct trace_seq *s,
					 struct tep_record *record,
					 struct tep_event *event, void *context);

#ifdef HAVE_UNITTEST
const char *ras_cxl_test_log_type(uint32_t log_type);
void ras_cxl_test_convert_timestamp(unsigned long long timestamp,
				    char *buf, uint16_t size);
const char *ras_cxl_test_uuid(const char *uuid);
#endif
struct ras_cxl_poison_event {
	char timestamp[64];
	const char *memdev;
	const char *host;
	uint64_t serial;
	const char *trace_type;
	const char *region;
	const char *uuid;
	uint64_t hpa;
	uint64_t hpa_alias0;
	uint64_t dpa;
	uint32_t dpa_length;
	const char *source;
	uint8_t flags;
	char overflow_ts[64];
};

#define CXL_HEADERLOG_SIZE              SZ_512
#define CXL_HEADERLOG_SIZE_U32          (SZ_512 / sizeof(uint32_t))
#define CXL_EVENT_RECORD_DATA_LENGTH	0x50
#define CXL_EVENT_GEN_MED_COMP_ID_SIZE	0x10
#define CXL_EVENT_DER_CORRECTION_MASK_SIZE	0x20

#define CXL_PLDM_ENTITY_ID_LEN	6
#define CXL_PLDM_RES_ID_LEN	4

struct ras_cxl_aer_ue_event {
	char timestamp[64];
	const char *memdev;
	const char *host;
	uint64_t serial;
	uint32_t error_status;
	uint32_t first_error;
	uint32_t *header_log;
};

struct ras_cxl_aer_ce_event {
	char timestamp[64];
	const char *memdev;
	const char *host;
	uint64_t serial;
	uint32_t error_status;
};

struct ras_cxl_overflow_event {
	char timestamp[64];
	const char *memdev;
	const char *host;
	uint64_t serial;
	const char *log_type;
	char first_ts[64];
	char last_ts[64];
	uint16_t count;
};

struct ras_cxl_event_common_hdr {
	char timestamp[64];
	const char *memdev;
	const char *host;
	uint64_t serial;
	const char *log_type;
	const char *hdr_uuid;
	uint32_t hdr_flags;
	uint16_t hdr_handle;
	uint16_t hdr_related_handle;
	char hdr_timestamp[64];
	uint8_t hdr_length;
	uint8_t hdr_maint_op_class;
	uint8_t hdr_maint_op_sub_class;
	uint16_t hdr_ld_id;
	uint8_t hdr_head_id;
};

struct ras_cxl_generic_event {
	struct ras_cxl_event_common_hdr hdr;
	uint8_t *data;
};

struct ras_cxl_general_media_event {
	struct ras_cxl_event_common_hdr hdr;
	uint64_t dpa;
	uint8_t dpa_flags;
	uint8_t descriptor;
	uint8_t type;
	uint8_t sub_type;
	uint8_t transaction_type;
	uint8_t channel;
	uint8_t rank;
	uint32_t device;
	uint8_t *comp_id;
	uint8_t entity_id[CXL_PLDM_ENTITY_ID_LEN];
	uint8_t res_id[CXL_PLDM_RES_ID_LEN];
	uint16_t validity_flags;
	uint64_t hpa;
	uint64_t hpa_alias0;
	const char *region;
	const char *region_uuid;
	uint8_t cme_threshold_ev_flags;
	uint32_t cme_count;
};

struct ras_cxl_dram_event {
	struct ras_cxl_event_common_hdr hdr;
	uint64_t dpa;
	uint8_t dpa_flags;
	uint8_t descriptor;
	uint8_t type;
	uint8_t sub_type;
	uint8_t transaction_type;
	uint8_t channel;
	uint8_t sub_channel;
	uint8_t rank;
	uint32_t nibble_mask;
	uint8_t bank_group;
	uint8_t bank;
	uint32_t row;
	uint16_t column;
	uint8_t *cor_mask;
	uint16_t validity_flags;
	uint64_t hpa;
	uint64_t hpa_alias0;
	const char *region;
	const char *region_uuid;
	uint8_t *comp_id;
	uint8_t entity_id[CXL_PLDM_ENTITY_ID_LEN];
	uint8_t res_id[CXL_PLDM_RES_ID_LEN];
	uint8_t cme_threshold_ev_flags;
	uint32_t cvme_count;
};

struct ras_cxl_memory_module_event {
	struct ras_cxl_event_common_hdr hdr;
	uint8_t event_type;
	uint8_t event_sub_type;
	uint8_t health_status;
	uint8_t media_status;
	uint8_t life_used;
	uint32_t dirty_shutdown_cnt;
	uint32_t cor_vol_err_cnt;
	uint32_t cor_per_err_cnt;
	int16_t device_temp;
	uint8_t add_status;
	uint16_t validity_flags;
	uint8_t *comp_id;
	uint8_t entity_id[CXL_PLDM_ENTITY_ID_LEN];
	uint8_t res_id[CXL_PLDM_RES_ID_LEN];
};
struct ras_cxl_memory_sparing_event {
	struct ras_cxl_event_common_hdr hdr;
	uint8_t flags;
	uint8_t result;
	uint16_t validity_flags;
	uint16_t res_avail;
	uint8_t channel;
	uint8_t rank;
	uint32_t nibble_mask;
	uint8_t bank_group;
	uint8_t bank;
	uint32_t row;
	uint16_t column;
	uint8_t sub_channel;
	uint8_t *comp_id;
	uint8_t entity_id[CXL_PLDM_ENTITY_ID_LEN];
	uint8_t res_id[CXL_PLDM_RES_ID_LEN];
};



#endif
