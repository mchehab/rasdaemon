/* SPDX-License-Identifier: GPL-2.0-or-later */

/*
 * Copyright (C) 2025 Alibaba Inc
 */

#ifndef __RAS_POISON_PAGE_STAT_H
#define __RAS_POISON_PAGE_STAT_H

extern unsigned long long poison_stat_threshold;

int ras_poison_page_stat(void);

#endif
