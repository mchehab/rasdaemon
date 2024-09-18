/* SPDX-License-Identifier: GPL-2.0-or-later */

/*
 * Copyright (c) 2020 Hisilicon Limited.
 */

#ifndef __NON_STANDARD_HISILICON_H
#define __NON_STANDARD_HISILICON_H

#include "ras-mc-handler.h"
#include "ras-non-standard-handler.h"

#define HISI_SNPRINTF	mce_snprintf

#define HISI_ERR_SEVERITY_NFE	0
#define HISI_ERR_SEVERITY_FE	1
#define HISI_ERR_SEVERITY_CE	2
#define HISI_ERR_SEVERITY_NONE	3

enum hisi_oem_data_type {
	HISI_OEM_DATA_TYPE_INT,
	HISI_OEM_DATA_TYPE_INT64,
	HISI_OEM_DATA_TYPE_TEXT,
};

/* helper functions */
static inline char *err_severity(uint8_t err_sev)
{
	switch (err_sev) {
	case HISI_ERR_SEVERITY_NFE: return "recoverable";
	case HISI_ERR_SEVERITY_FE: return "fatal";
	case HISI_ERR_SEVERITY_CE: return "corrected";
	case HISI_ERR_SEVERITY_NONE: return "none";
	default:
		break;
	}
	return "unknown";
}

void record_vendor_data(struct ras_ns_ev_decoder *ev_decoder,
			enum hisi_oem_data_type data_type,
			int id, int64_t data, const char *text);
int step_vendor_data_tab(struct ras_ns_ev_decoder *ev_decoder, const char *name);

#endif
