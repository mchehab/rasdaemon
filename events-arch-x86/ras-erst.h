/* SPDX-License-Identifier: GPL-2.0-or-later */

/*
 * Copyright (C) 2025 Alibaba Inc
 */

#ifndef __RAS_ERST_H
#define __RAS_ERST_H

#define ERST_DELETE	"ERST_DELETE"

struct mce_event;

void handle_erst(void);
#ifdef HAVE_UNITTEST
int ras_erst_test_read(const char *path, struct mce_event *event);
#endif
#endif
