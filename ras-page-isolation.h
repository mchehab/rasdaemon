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

#include <sys/types.h>
#include <sys/queue.h>
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

enum row_location_type{
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

struct memory_location_field {
	const char	*name;
	const char	*anchor_str;
	const int	value_base;
};
extern const struct memory_location_field apei_fields[];
extern const struct memory_location_field dsm_fields[];

struct page_addr {
	LIST_ENTRY(page_addr)	entry;
	unsigned long long	addr;
	enum pstate		offlined;
	int				count;
	time_t			start;
};

#define ROW_LOCATION_FIELDS_NUM (DSM_FIELD_NUM > APEI_FIELD_NUM ? DSM_FIELD_NUM : APEI_FIELD_NUM)
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
void ras_record_page_error(unsigned long long addr, unsigned count, time_t time);
void ras_row_account_init(void);
void ras_record_row_error(const char *detail, unsigned count, time_t time, unsigned long long addr);

void row_record_get_id(struct row_record *rr, char *buffer);
bool row_record_is_same_row(struct row_record *rr1, struct row_record *rr2);
void row_record_copy(struct row_record *dst, struct row_record *src);
void row_record_free(struct row_record *rr);
void row_record_infos_free(void);

#endif
