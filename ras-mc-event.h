/*
 * Copyright (C) 2013 Mauro Carvalho Chehab <mchehab@redhat.com>
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

#include "ras-record.h"

#include <pthread.h>

#ifndef __RAS_MC_EVENT_H
#define __RAS_MC_EVENT_H
struct ras_events {
	struct pevent	*pevent;
	int		page_size;

	/* For ras-record */
	sqlite3		*db;
	sqlite3_stmt	*stmt;
};

struct pthread_data {
	pthread_t		thread;
	struct pevent		*pevent;
	struct ras_events	*ras;
	int			cpu;
};

int handle_ras_events(int record_events);
int toggle_ras_mc_event(int enable);

#endif