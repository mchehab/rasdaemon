/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2025 Ruidong Tian <tianruidong@linux.alibaba.com>
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
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef __RAS_SIGNAL_HANDLER_H
#define __RAS_SIGNAL_HANDLER_H

#include <sys/types.h>
#include <traceevent/event-parse.h>

#include "core/ras-events.h"

struct ras_signal_event {
	char timestamp[64];
	int sig;
	int error_no;
	int code;
	char *comm;
	pid_t pid;
	int group;
	int result;
};

#endif
