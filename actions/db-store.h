/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2026 Mauro Carvalho Chehab <mchehab+huawei@kernel.org>
 */

#ifndef DB_STORE_H
#define DB_STORE_H

struct ras_events;

int db_store_tables_open(struct ras_events *ras, unsigned int cpu);
int db_store_tables_close(unsigned int cpu);

#endif /* DB_STORE_H */
