/* SPDX-License-Identifier: GPL-2.0-or-later */

/*
 * Copyright (C) 2013 Mauro Carvalho Chehab <mchehab+huawei@kernel.org>
 * Copyright (c) 2016, The Linux Foundation. All rights reserved.
 */

#ifndef __RAS_RECORD_H
#define __RAS_RECORD_H

#include "config.h"

#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>

#include "core/types.h"
#include "db/ras-db.h"

struct ras_events;

struct ras_mc_event {
	char timestamp[64];
	int error_count;
	const char *error_type, *msg, *label;
	unsigned char mc_index;
	signed char top_layer, middle_layer, lower_layer;
	unsigned long long address, grain, syndrome;
	const char *driver_detail;
	int erst;
};

struct ras_mc_offline_event {
	unsigned int family, model;
	bool smca;
	uint8_t bank;
	uint64_t ipid;
	uint64_t synd;
	uint64_t status;
};

struct ras_aer_event {
	char timestamp[64];
	const char *error_type;
	char *dev_name;
	uint8_t tlp_header_valid;
	uint32_t *tlp_header;
	const char *msg;
	int erst;
	uint16_t vendor_id;
	uint16_t device_id;
};

struct ras_extlog_event {
	char timestamp[64];
	int32_t error_seq;
	int8_t etype;
	int8_t severity;
	unsigned long long address;
	int8_t pa_mask_lsb;
	const char *fru_id;
	const char *fru_text;
	const char *cper_data;
	unsigned short cper_data_length;
};

struct ras_non_standard_event {
	char timestamp[64];
	const char *sec_type, *fru_id, *fru_text;
	const char *severity;
	const uint8_t *error;
	uint32_t length;
};

struct ras_arm_event {
	char timestamp[64];
	int32_t error_count;
	int8_t affinity;
	int64_t mpidr;
	int64_t midr;
	int32_t running_state;
	int32_t psci_state;
	const uint8_t *pei_error;
	uint32_t pei_len;
	const uint8_t *ctx_error;
	uint32_t ctx_len;
	const uint8_t *vsei_error;
	uint32_t oem_len;
	char error_types[512];
	char error_flags[512];
	uint64_t error_info;
	uint64_t virt_fault_addr;
	uint64_t phy_fault_addr;
};

struct devlink_event {
	char timestamp[64];
	const char *bus_name;
	const char *dev_name;
	const char *driver_name;
	const char *reporter_name;
	char *msg;
};

struct diskerror_event {
	char timestamp[64];
	char *dev;
	unsigned long long sector;
	unsigned int nr_sector;
	const char *error;
	const char *rwbs;
	const char *cmd;
};

struct ras_mf_event {
	char timestamp[64];
	char pfn[30];
	const char *page_type;
	const char *action_result;
};

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

struct ras_signal_event {
	char timestamp[64];
	int sig;
	int error_no;
	int code;
	char *comm;
	pid_t pid;
	int group;
	int result;
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

struct ras_mc_event;
struct ras_aer_event;
struct ras_extlog_event;
struct ras_non_standard_event;
struct ras_arm_event;
struct mce_event;
struct devlink_event;
struct diskerror_event;
struct ras_mf_event;
struct ras_cxl_poison_event;
struct ras_cxl_aer_ue_event;
struct ras_cxl_aer_ce_event;
struct ras_cxl_overflow_event;
struct ras_cxl_generic_event;
struct ras_cxl_general_media_event;
struct ras_cxl_dram_event;
struct ras_cxl_memory_module_event;
struct ras_signal_event;
struct ras_cxl_memory_sparing_event;
struct ras_reri_event;

struct ras_record_priv {
	struct ras_stmt	*stmt_mc_event;
	/* AER */
	struct ras_stmt	*stmt_aer_event;

	/* MCE */
	struct ras_stmt	*stmt_mce_record;

	/* EXTLOG */
	struct ras_stmt	*stmt_extlog_record;

	/* NON_STANDARD */
	struct ras_stmt	*stmt_non_standard_record;

	/* ARM */
	struct ras_stmt	*stmt_arm_record;

	/* DEVLINK */
	struct ras_stmt	*stmt_devlink_event;

	/* DISKERROR */
	struct ras_stmt	*stmt_diskerror_event;

	/* MEMORY_FAILURE */
	struct ras_stmt	*stmt_mf_event;

	/* CXL */
	struct ras_stmt	*stmt_cxl_poison_event;
	struct ras_stmt	*stmt_cxl_aer_ue_event;
	struct ras_stmt	*stmt_cxl_aer_ce_event;
	struct ras_stmt	*stmt_cxl_overflow_event;
	struct ras_stmt	*stmt_cxl_generic_event;
	struct ras_stmt	*stmt_cxl_general_media_event;
	struct ras_stmt	*stmt_cxl_dram_event;
	struct ras_stmt	*stmt_cxl_memory_module_event;

	/* SIGNAL */
	struct ras_stmt	*stmt_signal_event;

	/* RERI */
	struct ras_stmt	*stmt_reri_event;
};


#ifdef HAVE_DB
int ras_mc_event_opendb(unsigned int cpu, struct ras_events *ras);
int ras_mc_event_closedb(unsigned int cpu, struct ras_events *ras);

int db_mc_event(struct ras_events *ras, struct ras_mc_event *ev);
int db_aer_event(struct ras_events *ras, struct ras_aer_event *ev);
int db_mce_record(struct ras_events *ras, struct mce_event *ev);
int db_extlog_mem_record(struct ras_events *ras,
				struct ras_extlog_event *ev);
int db_non_standard_record(struct ras_events *ras,
				  struct ras_non_standard_event *ev);
int db_arm_record(struct ras_events *ras, struct ras_arm_event *ev);
int db_devlink_event(struct ras_events *ras, struct devlink_event *ev);
int db_diskerror_event(struct ras_events *ras,
			      struct diskerror_event *ev);
int db_mf_event(struct ras_events *ras, struct ras_mf_event *ev);
int db_cxl_poison_event(struct ras_events *ras,
			       struct ras_cxl_poison_event *ev);
int db_cxl_aer_ue_event(struct ras_events *ras,
			       struct ras_cxl_aer_ue_event *ev);
int db_cxl_aer_ce_event(struct ras_events *ras,
			       struct ras_cxl_aer_ce_event *ev);
int db_cxl_overflow_event(struct ras_events *ras,
				 struct ras_cxl_overflow_event *ev);
int db_cxl_generic_event(struct ras_events *ras,
				struct ras_cxl_generic_event *ev);
int db_cxl_general_media_event(struct ras_events *ras,
				      struct ras_cxl_general_media_event *ev);
int db_cxl_dram_event(struct ras_events *ras,
			     struct ras_cxl_dram_event *ev);
int db_cxl_memory_module_event(struct ras_events *ras,
				      struct ras_cxl_memory_module_event *ev);
int db_signal_event(struct ras_events *ras,
			   struct ras_signal_event *ev);
int db_reri_event(struct ras_events *ras, struct ras_reri_event *ev);

#else
static inline int ras_mc_event_opendb(unsigned int cpu,
				      struct ras_events *ras) { return 0; };
static inline int ras_mc_event_closedb(unsigned int cpu,
				       struct ras_events *ras) { return 0; };
static inline int db_mc_event(struct ras_events *ras,
				     struct ras_mc_event *ev) { return 0; };
static inline int db_aer_event(struct ras_events *ras,
				      struct ras_aer_event *ev) { return 0; };
static inline int db_mce_record(struct ras_events *ras,
				       struct mce_event *ev) { return 0; };
static inline int db_extlog_mem_record(struct ras_events *ras,
					      struct ras_extlog_event *ev) { return 0; };
static inline int db_non_standard_record(struct ras_events *ras,
						struct ras_non_standard_event *ev) { return 0; };
static inline int db_arm_record(struct ras_events *ras,
				       struct ras_arm_event *ev) { return 0; };
static inline int db_devlink_event(struct ras_events *ras,
					  struct devlink_event *ev) { return 0; };
static inline int db_diskerror_event(struct ras_events *ras,
					    struct diskerror_event *ev) { return 0; };
static inline int db_mf_event(struct ras_events *ras,
				     struct ras_mf_event *ev) { return 0; };
static inline int db_cxl_poison_event(struct ras_events *ras,
					     struct ras_cxl_poison_event *ev) { return 0; };
static inline int db_cxl_aer_ue_event(struct ras_events *ras,
					     struct ras_cxl_aer_ue_event *ev) { return 0; };
static inline int db_cxl_aer_ce_event(struct ras_events *ras,
					     struct ras_cxl_aer_ce_event *ev) { return 0; };
static inline int db_cxl_overflow_event(struct ras_events *ras,
					       struct ras_cxl_overflow_event *ev) { return 0; };
static inline int db_cxl_generic_event(struct ras_events *ras,
					      struct ras_cxl_generic_event *ev) { return 0; };
static inline int db_cxl_general_media_event(struct ras_events *ras,
						    struct ras_cxl_general_media_event *ev) { return 0; };
static inline int db_cxl_dram_event(struct ras_events *ras,
					   struct ras_cxl_dram_event *ev) { return 0; };
static inline int db_cxl_memory_module_event(struct ras_events *ras,
						    struct ras_cxl_memory_module_event *ev) { return 0; };
static inline int db_signal_event(struct ras_events *ras,
					 struct ras_signal_event *ev) { return 0; };
static inline int db_reri_event(struct ras_events *ras,
				       struct ras_reri_event *ev) { return 0; };

#endif

#endif
