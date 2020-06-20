/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2020-2020. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
*/

#ifndef __RAS_PAGE_ISOLATION_H
#define __RAS_PAGE_ISOLATION_H

#include <time.h>
#include <stdbool.h>
#include "rbtree.h"

#define PAGE_SHIFT		12
#define PAGE_SIZE		(1 << PAGE_SHIFT)
#define PAGE_MASK		(~(PAGE_SIZE-1))

struct config {
	char			*name;
	unsigned long   val;
};

enum otype {
	OFFLINE_OFF,
	OFFLINE_ACCOUNT,
	OFFLINE_SOFT,
	OFFLINE_HARD,
	OFFLINE_SOFT_THEN_HARD,
};

enum pstate {
	PAGE_ONLINE,
	PAGE_OFFLINE,
	PAGE_OFFLINE_FAILED,
};

struct page_record {
	struct rb_node		entry;
	unsigned long long	addr;
	time_t			start;
	enum pstate		offlined;
	unsigned long		count;
	unsigned long		excess;
};

struct isolation {
	char			*name;
	char			*env;
	const struct config	*units;
	unsigned long		val;
	bool			overflow;
	char			*unit;
};

void ras_page_account_init(void);
void ras_record_page_error(unsigned long long addr, unsigned count, time_t time);

#endif
