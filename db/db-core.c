// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2026 Mauro Carvalho Chehab <mchehab+huawei@kernel.org>
 */

#include "core/modules.h"
#include "core/ras-events.h"
#include "core/ras-logger.h"
#include "db/db-core.h"
#include "db/ras-db.h"

static int close_status;

static int db_core_init(struct ras_module_ctx *ctx)
{
	int rc;

	rc = db_backend_enable(NULL);
	if (rc)
		return rc;

	if (!ctx->ras->record_events)
		return 0;

	rc = db_open(NULL, 0, ctx->ras, 0);
	if (rc)
		log(TERM, LOG_ERR, "Failed to open SQL database\n");

	return rc;
}

static void db_core_cleanup(struct ras_module_ctx *ctx)
{
	close_status = db_close(0, ctx->ras);
	if (close_status)
		log(TERM, LOG_ERR, "Failed to close SQL database\n");
}

static const struct ras_module_entry db_core_module = {
	.name = "db-core",
	.level = DB_CORE_MODULE,
	.init = db_core_init,
	.cleanup = db_core_cleanup,
};

REGISTER_RAS_MODULE(db_core_module);

int db_core_status(void)
{
	return close_status;
}
