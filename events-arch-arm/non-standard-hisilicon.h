/* SPDX-License-Identifier: GPL-2.0-or-later */

/*
 * Copyright (c) 2020 Hisilicon Limited.
 */

#ifndef __NON_STANDARD_HISILICON_H
#define __NON_STANDARD_HISILICON_H

#include "db/ras-db.h"
#include "events-arch-arm/ras-non-standard-handler.h"
#include "events/ras-mc-handler.h"

#define HISI_SNPRINTF	mce_snprintf

#define HISI_ERR_SEVERITY_NFE	0
#define HISI_ERR_SEVERITY_FE	1
#define HISI_ERR_SEVERITY_CE	2
#define HISI_ERR_SEVERITY_NONE	3

/* helper functions */
static inline const char *err_severity(uint8_t err_sev)
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

#endif
