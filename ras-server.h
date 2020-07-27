/*
 * Copyright (C) 2020 Alexandre de Limas Santana <alexandre.delimassantana@bsc.es>
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

#ifndef __RAS_SERVER_H
#define __RAS_SERVER_H

#include "config.h"
#include "ras-events.h"
#include "ras-mc-handler.h"
#include "ras-mce-handler.h"
#include "ras-aer-handler.h"

/* RASdaemon Socket name (abstract namespace). */
#define SOCKET_NAME "rasdaemon"
/* Maximum number of active clients. */
#define SERVER_MAX_CONN 10
/* Maximum message size in bytes. */
#define SRV_MSG_SIZE (8192)

struct ras_server {
	pthread_t tid;					// The server thread id
	pthread_mutex_t fd_mutex;		// Mutex for client file descriptors
	int socketfd;					// The local Unix socket file descriptor
	struct pollfd *fds;				// Connected socket and server socket fds
	int nclients;					// The maximum number of concurrent clients
};

#ifdef HAVE_BROADCAST

int ras_server_start(void);
void ras_server_broadcast(int type, void *ev);
void ras_server_stop(void);

#else

inline int ras_server_start(void) { return 0; }
inline void ras_server_broadcast(int type, void *ev) {}
inline void ras_server_stop(void) {}

#endif

#endif
