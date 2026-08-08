/* SPDX-License-Identifier: GPL-2.0-or-later */

/*
 * Copyright (C) 2025 Alibaba Inc
 */

#ifndef __RAS_ERST_H
#define __RAS_ERST_H

#define ERST_DELETE	"ERST_DELETE"

#ifdef HAVE_MCE
void handle_erst_mce(void);
#endif

void handle_erst(void);
#endif
