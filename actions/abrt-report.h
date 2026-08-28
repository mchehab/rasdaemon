/* SPDX-License-Identifier: GPL-2.0-only */

/*
 * Copyright (c) 2016, The Linux Foundation. All rights reserved.
 */

#ifndef __ABRT_REPORT_H
#define __ABRT_REPORT_H

/*
 * ABRT (Automatic Bug Reporting Tool) report consumer.
 *
 * Production event delivery is private to actions/abrt-report.c: the action
 * registers itself as an event consumer and sends supported decoded RAS
 * events to ABRT's local Unix socket. Event producers publish through
 * ras_event_publish() and do not include this header.
 */

#include <stddef.h>

#ifdef HAVE_UNITTEST
int abrt_report_test_format(int type, void *event, char *output, size_t size);
#endif

#endif /* __ABRT_REPORT_H */
