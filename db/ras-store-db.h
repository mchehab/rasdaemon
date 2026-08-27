/* SPDX-License-Identifier: GPL-2.0-or-later */

/*
 * Copyright (C) 2013 Mauro Carvalho Chehab <mchehab+huawei@kernel.org>
 * Copyright (c) 2016, The Linux Foundation. All rights reserved.
 */

#ifndef __RAS_RECORD_H
#define __RAS_RECORD_H

#include "config.h"

#include "db/ras-db.h"

struct ras_events;

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
};


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

#ifdef HAVE_DB
int ras_mc_event_opendb(unsigned int cpu, struct ras_events *ras);
int ras_mc_event_closedb(unsigned int cpu, struct ras_events *ras);
int db_mc_event(struct ras_events *ras, struct ras_mc_event *ev);

#else
static inline int ras_mc_event_opendb(unsigned int cpu,
				      struct ras_events *ras) { return 0; };
static inline int ras_mc_event_closedb(unsigned int cpu,
				       struct ras_events *ras) { return 0; };
static inline int db_mc_event(struct ras_events *ras,
				     struct ras_mc_event *ev) { return 0; };

#endif

#endif
