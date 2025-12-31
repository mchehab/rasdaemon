/* SPDX-License-Identifier: GPL-2.0 */

/*
 * Copyright (c) 2016, The Linux Foundation. All rights reserved.
 */

#ifndef __RAS_REPORT_H
#define __RAS_REPORT_H

#include "ras-aer-handler.h"
#include "ras-events.h"
#include "ras-mce-handler.h"
#include "ras-mc-handler.h"
#include "ras-record.h"
#include "types.h"

/* Maximal length of backtrace. */
#define MAX_BACKTRACE_SIZE (1024 * 1024)
/* Amount of data received from one client for a message before reporting error. */
#define MAX_MESSAGE_SIZE (4 * MAX_BACKTRACE_SIZE)
/* Maximal number of characters read from socket at once. */
#define INPUT_BUFFER_SIZE (8 * 1024)
/* ABRT socket file */
#define ABRT_SOCKET "/var/run/abrt/abrt.socket"

#ifdef HAVE_ABRT_REPORT

int ras_report_mc_event(struct ras_events *ras,
			struct ras_mc_event *ev);
int ras_report_aer_event(struct ras_events *ras,
			 struct ras_aer_event *ev);
int ras_report_mce_event(struct ras_events *ras,
			 struct mce_event *ev);
int ras_report_non_standard_event(struct ras_events *ras,
				  struct ras_non_standard_event *ev);
int ras_report_arm_event(struct ras_events *ras,
			 struct ras_arm_event *ev);
int ras_report_devlink_event(struct ras_events *ras,
			     struct devlink_event *ev);
int ras_report_diskerror_event(struct ras_events *ras,
			       struct diskerror_event *ev);
int ras_report_mf_event(struct ras_events *ras,
			struct ras_mf_event *ev);
int ras_report_cxl_poison_event(struct ras_events *ras,
				struct ras_cxl_poison_event *ev);
int ras_report_cxl_aer_ue_event(struct ras_events *ras,
				struct ras_cxl_aer_ue_event *ev);
int ras_report_cxl_aer_ce_event(struct ras_events *ras,
				struct ras_cxl_aer_ce_event *ev);
int ras_report_cxl_overflow_event(struct ras_events *ras,
				  struct ras_cxl_overflow_event *ev);
int ras_report_cxl_generic_event(struct ras_events *ras,
				 struct ras_cxl_generic_event *ev);
int ras_report_cxl_general_media_event(struct ras_events *ras,
				       struct ras_cxl_general_media_event *ev);
int ras_report_cxl_dram_event(struct ras_events *ras,
			      struct ras_cxl_dram_event *ev);
int ras_report_cxl_memory_module_event(struct ras_events *ras,
				       struct ras_cxl_memory_module_event *ev);
int ras_report_signal_event(struct ras_events *ras,
			    struct ras_signal_event *ev);

#else

static inline int ras_report_mc_event(struct ras_events *ras,
				      struct ras_mc_event *ev)
{ return 0; };
static inline int ras_report_aer_event(struct ras_events *ras,
				       struct ras_aer_event *ev)
{ return 0; };
static inline int ras_report_mce_event(struct ras_events *ras,
				       struct mce_event *ev)
{ return 0; };
static inline int ras_report_non_standard_event(struct ras_events *ras,
						struct ras_non_standard_event *ev)
{ return 0; };
static inline int ras_report_arm_event(struct ras_events *ras,
				       struct ras_arm_event *ev)
{ return 0; };
static inline int ras_report_devlink_event(struct ras_events *ras,
					   struct devlink_event *ev)
{ return 0; };
static inline int ras_report_diskerror_event(struct ras_events *ras,
					     struct diskerror_event *ev)
{ return 0; };
static inline int ras_report_mf_event(struct ras_events *ras,
				      struct ras_mf_event *ev)
{ return 0; };
static inline int ras_report_cxl_poison_event(struct ras_events *ras,
					      struct ras_cxl_poison_event *ev)
{ return 0; };
static inline int ras_report_cxl_aer_ue_event(struct ras_events *ras,
					      struct ras_cxl_aer_ue_event *ev)
{ return 0; };
static inline int ras_report_cxl_aer_ce_event(struct ras_events *ras,
					      struct ras_cxl_aer_ce_event *ev)
{ return 0; };
static inline int ras_report_cxl_overflow_event(struct ras_events *ras,
						struct ras_cxl_overflow_event *ev)
{ return 0; };
static inline int ras_report_cxl_generic_event(struct ras_events *ras,
					       struct ras_cxl_generic_event *ev)
{ return 0; };
static inline int ras_report_cxl_general_media_event(struct ras_events *ras,
						     struct ras_cxl_general_media_event *ev)
{ return 0; };
static inline int ras_report_cxl_dram_event(struct ras_events *ras,
					    struct ras_cxl_dram_event *ev)
{ return 0; };
static inline int ras_report_cxl_memory_module_event(struct ras_events *ras,
						     struct ras_cxl_memory_module_event *ev)
{ return 0; };
static inline int ras_report_signal_event(struct ras_events *ras,
					  struct ras_signal_event *ev)
{ return 0; };
#endif

#endif
