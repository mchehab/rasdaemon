/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2026 Mauro Carvalho Chehab <mchehab+huawei@kernel.org>
 */

#include "config.h"

#ifndef RAS_DB_CORE_H
#define RAS_DB_CORE_H

#ifdef HAVE_DB
int db_core_status(void);
#else
static inline int db_core_status(void)
{
	return 0;
}
#endif

#endif
