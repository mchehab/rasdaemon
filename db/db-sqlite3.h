/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2026 Mauro Carvalho Chehab <mchehab+huawei@kernel.org>
 */

struct db_sqlite3_conn_params {
	const char *fname;
	const char *dir;
	int extra_flags;
};
