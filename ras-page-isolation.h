/* SPDX-License-Identifier: GPL-2.0-or-later */

/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2020-2020. All rights reserved.
 */

#ifndef __RAS_PAGE_ISOLATION_H
#define __RAS_PAGE_ISOLATION_H

#include <sys/types.h>
#include <sys/queue.h>
#include <stdbool.h>
#include <time.h>

#include "rbtree.h"
#include "types.h"

#define PAGE_SHIFT		12
#define PAGE_SIZE		BIT(PAGE_SHIFT)
#define PAGE_MASK		(~(PAGE_SIZE - 1))

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

enum row_location_type {
	GHES,
	DSM
};

enum apei_location_field_index {
	APEI_NODE,
	APEI_CARD,
	APEI_MODULE,
	APEI_RANK,
	APEI_DEVICE,
	APEI_BANK,
	APEI_ROW,
	APEI_FIELD_NUM
};

enum dsm_location_field_index {
	DSM_ProcessorSocketId,
	DSM_MemoryControllerId,
	DSM_ChannelId,
	DSM_DimmSlotId,
	DSM_PhysicalRankId,
	DSM_ChipId,
	DSM_BankGroup,
	DSM_Bank,
	DSM_Row,
	DSM_FIELD_NUM
};

#define  APEI_FIELD_NUM_CONST ((int)APEI_FIELD_NUM)
#define  DSM_FIELD_NUM_CONST ((int)DSM_FIELD_NUM)

struct memory_location_field {
	const char	*name;
	const char	*anchor_str;
	const int	value_base;
};

struct page_addr {
	LIST_ENTRY(page_addr)	entry;
	unsigned long long	addr;
	enum pstate		offlined;
	int			count;
	time_t			start;
};

#define ROW_LOCATION_FIELDS_NUM (DSM_FIELD_NUM_CONST > APEI_FIELD_NUM_CONST ? \
				 DSM_FIELD_NUM_CONST : APEI_FIELD_NUM_CONST)

struct row_record {
	LIST_ENTRY(row_record)	entry;
	LIST_HEAD(page_listhead, page_addr)	page_head;
	enum row_location_type	type;
	int			location_fields[ROW_LOCATION_FIELDS_NUM];
	time_t			start;
	unsigned long		count;
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
void ras_record_page_error(unsigned long long addr,
			   unsigned int count, time_t time);
void ras_hw_threshold_pageoffline(unsigned long long addr);
void ras_row_account_init(void);
void ras_record_row_error(const char *detail, unsigned int count, time_t time,
			  unsigned long long addr);
void row_record_infos_free(void);

#endif
