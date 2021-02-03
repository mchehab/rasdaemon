#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "ras-server.h"
#include "ras-logger.h"

static struct ras_server server;

static void ras_server_cleanup(void* arg) {
	// Wait until main thread finishes current broadcast
	pthread_mutex_lock(&server.fd_mutex);
	server.nclients = 0;  // Prevent future broadcasts
	pthread_mutex_unlock(&server.fd_mutex);
	// De-initialize the remainder of the server attributes
	pthread_mutex_destroy(&server.fd_mutex);
	if(server.socketfd > 0) {
		shutdown(server.socketfd, SHUT_RDWR);
		close(server.socketfd);
	}
	if(server.fds)
		free(server.fds);
	memset(&server, 0, sizeof(struct ras_server));
	signal(SIGPIPE, SIG_DFL);
	log(ALL, LOG_INFO, "RAS server has stoped\n");
}

static void cleanup_unlock_mutex(void *p)
{
    pthread_mutex_unlock(p);
}

static void server_loop(void) {
	int next_slot, server_full, rv, i;

	pthread_cleanup_push(ras_server_cleanup, NULL);
	server_full = 0;
	next_slot = 0;

	while(1) {
		rv = poll(server.fds, server.nclients+1, -1);
		if(rv < 0) {
			log(ALL, LOG_ERR, "Can't poll the connection file descriptors\n");
			break;
		}
		
		pthread_mutex_lock(&server.fd_mutex);
		pthread_cleanup_push(cleanup_unlock_mutex, &server.fd_mutex);
		for(i = 0; i <= server.nclients; ++i) {
			struct pollfd *p = &server.fds[i];
			int clifd = 0;
			
			// Check if all poll() notifications have been served
			if(!rv)
				break;
			// If a connection closed, release resources
			if(p->revents & POLLHUP) {
				--rv;
				close(p->fd);
				p->fd = -1;    // stop tracking in poll()
				next_slot = i; // This slot is free
				// Poll the socket if it there are connection slots
				if(server_full) {
					server_full = 0;
					server.fds[server.nclients].fd = server.socketfd;
					server.fds[server.nclients].events = POLLIN;
				}
				log(ALL, LOG_INFO, "Client %d disconnected from RAS server\n",
					next_slot);
			}
			// If a connection opened, set-up context and invoke the handler
			else if(p->revents & POLLIN) {
				--rv;
				// Find the next connection slot
				if(next_slot < 0) {
					for(int w = 0; w < server.nclients; ++w) {
						if(server.fds[w].fd < 0) {
							next_slot = w;
							break;
						}
					}
				}
				clifd = accept(server.socketfd, NULL, NULL);
				if(clifd < 0) {
					log(ALL, LOG_WARNING, "Can't accept client connection\n");
					continue;
				}
				// No more connection slots, stop polling the server socket
				if(next_slot < 0) {
					server.fds[server.nclients].fd = -1;
					server_full = 1;
					close(clifd);
				} else {
					server.fds[next_slot].fd = clifd;
					next_slot = -1;
					log(ALL, LOG_INFO, "Client %d connected to RAS server\n",
						next_slot);
				}
			}
		}
		pthread_cleanup_pop(1); // Call pthread_mutex_unlock
	}
	pthread_cleanup_pop(1); // Call ras_server_cleanup
}

void ras_server_stop(void) {
	if(server.tid > 0)
		pthread_cancel(server.tid);
}

int ras_server_start(void) {
	struct sockaddr_un addr;

	server.socketfd = socket(AF_UNIX, SOCK_STREAM, 0);
	if(server.socketfd == -1) {
		log(ALL, LOG_WARNING, "Can't create local socket for broadcasting\n");
		goto create_err;
	}

	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	addr.sun_path[0] = '\0';
	strncpy(addr.sun_path+1, SOCKET_NAME, sizeof(addr.sun_path)-2);
	
	if(bind(server.socketfd, (struct sockaddr*)&addr, sizeof(addr))){
		log(ALL, LOG_WARNING, "Can't bind to local socket for broadcasting\n");
		goto create_err;
	}
	if(listen(server.socketfd, SERVER_MAX_CONN)) {
		log(ALL, LOG_WARNING, "Can't listen on local socket for broadcasting\n");
		goto create_err;
	}

	server.nclients = SERVER_MAX_CONN;
	server.fds = calloc(server.nclients+1, sizeof(struct pollfd));
	// Set up socket watchers in poll()
	server.fds[server.nclients].fd = server.socketfd;
	server.fds[server.nclients].events = POLLIN;
	// Set up client connection watchers in poll()
	for(int i = 0; i < server.nclients; ++i) {
		server.fds[i].fd = -1;
		server.fds[i].events = POLLHUP;
	}

	if(pthread_mutex_init(&server.fd_mutex, NULL)) {
		log(ALL, LOG_WARNING, "Can't create server mutex\n");
		goto create_err;
	}
	if(pthread_create(&server.tid, NULL, (void*(*)(void*))server_loop, NULL)) {
		log(ALL, LOG_WARNING, "Can't create server thread\n");
		goto create_err;
	}

	signal(SIGPIPE, SIG_IGN);
	log(ALL, LOG_INFO, "Server is listening to connections\n");
	return 0;

	create_err:
	ras_server_stop();
	return -1;
}

// Serialization function for MC events
static inline void ras_mc_msg(char *buf, struct ras_mc_event *ev) {
	snprintf(buf, SRV_MSG_SIZE, "type=mc,"
		"timestamp=%s,"
		"error_count=%d,"
		"error_type=%s,"
		"msg=%s,"
		"label=%s,"
		"mc_index=%c,"
		"top_layer=%c,"
		"middle_layer=%c,"
		"lower_layer=%c,"
		"address=%llu,"
		"grain=%llu,"
		"syndrome=%llu,"
		"driver_detail=%s",
		ev->timestamp,
		ev->error_count,
		ev->error_type,
		ev->msg,
		ev->label,
		ev->mc_index,
		ev->top_layer,
		ev->middle_layer,
		ev->lower_layer,
		ev->address,
		ev->grain,
		ev->syndrome,
		ev->driver_detail);
}

// Serialization function for AER events
static inline void ras_aer_msg(char *buf, struct ras_aer_event *ev) {
	snprintf(buf, SRV_MSG_SIZE, "type=aer,"
		"timestamp=%s,"
		"error_type=%s,"
		"dev_name=%s,"
		"msg=%s",
		ev->timestamp,
		ev->error_type,
		ev->dev_name,
		ev->msg);
}

// Serialization function for MCE events
static inline void ras_mce_msg(char *buf, struct mce_event *ev) {
	snprintf(buf, SRV_MSG_SIZE, "type=mce,"
		"timestamp=%s,"
		"bank_name=%s,"
		"mc_location=%s,"
		"error_msg=%s",
		ev->timestamp,
		ev->bank_name,
		ev->mc_location,
		ev->error_msg);
}

// Serialization function for non_standard events
static inline void ras_non_standard_msg(char *buf,
										struct ras_non_standard_event *ev) {
	snprintf(buf, SRV_MSG_SIZE, "type=non_standard,"
		"timestamp=%s,"
		"severity=%s,"
		"length=%d",
		ev->timestamp,
		ev->severity,
		ev->length);
}

// Serialization function for ARM events
static inline void ras_arm_msg(char *buf, struct ras_arm_event *ev) {
	snprintf(buf, SRV_MSG_SIZE, "type=arm,"
		"timestamp=%s,"
		"error_count=%d,"
		"affinity=%d,"
		"mpidr=0x%lx,"
		"midr=0x%lx,"
		"running_state=%d,"
		"psci_state=%d",
		ev->timestamp,
		ev->error_count,
		ev->affinity,
		ev->mpidr,
		ev->midr,
		ev->running_state,
		ev->psci_state);
}

// Serialization function for devlink events
static inline void ras_devlink_msg(char *buf, struct devlink_event *ev) {
	snprintf(buf, SRV_MSG_SIZE, "type=devlink,"
		"timestamp=%s,"
		"bus_name=%s,"	
		"dev_name=%s,"	
		"driver_name=%s,"
		"reporter_name=%s,"
		"msg=%s",
		ev->timestamp,
		ev->bus_name,
		ev->dev_name,
		ev->driver_name,
		ev->reporter_name,
		ev->msg);
}

// Serialization function for diskerror events
static inline void ras_diskerror_msg(char *buf, struct diskerror_event *ev) {
	snprintf(buf, SRV_MSG_SIZE, "type=diskerror,"
		"timestamp=%s,"
		"dev=%s,"
		"sector=%llu,"
		"nr_sector=%u,"
		"error=%s,"
		"rwbs=%s,"
		"cmd=%s",
		ev->timestamp,
		ev->dev,
		ev->sector,
		ev->nr_sector,
		ev->error,
		ev->rwbs,
		ev->cmd);
}

void ras_server_broadcast(int type, void *ev) {
	char msg[SRV_MSG_SIZE];
	size_t sent[SERVER_MAX_CONN];
	int conn_id[SERVER_MAX_CONN];
	int nconn, done, i, rv;
	size_t size;

	if(!server.nclients)
		return;
	// Filter the connection slots to get only the active connections
	nconn = 0;
	done = 0;
	pthread_mutex_lock(&server.fd_mutex);
	for(i = 0; i < server.nclients; ++i) {
		if(server.fds[i].fd != -1) {
			conn_id[nconn] = i;
			sent[nconn] = 0;
			nconn++;
		}
	}
	pthread_mutex_unlock(&server.fd_mutex);
	// Nothing to do if no event or no connections
	if(!nconn || !ev)
		return;

	// Construct the notification message
	switch (type)
	{
	case MC_EVENT:
		ras_mc_msg(msg, (struct ras_mc_event *)ev);
		break;
	case AER_EVENT:
		ras_aer_msg(msg, (struct ras_aer_event *)ev);
		break;
	case MCE_EVENT:
		ras_mce_msg(msg, (struct mce_event *)ev);
		break;
	case NON_STANDARD_EVENT:
		ras_non_standard_msg(msg, (struct ras_non_standard_event *)ev);
		break;
	case ARM_EVENT:
		ras_arm_msg(msg, (struct ras_arm_event *)ev);
		break;
	case DEVLINK_EVENT:
		ras_devlink_msg(msg, (struct devlink_event *)ev);
		break;
	case DISKERROR_EVENT:
		ras_diskerror_msg(msg, (struct diskerror_event *)ev);
		break;
	default:
		return;
	}
	// Send message to all clients
	size = strlen(msg);
	while(!done) {
		done = 1;
		for(i = 0; i < nconn; ++i) {
			// Skip if the client already got the data or failed to get it
			if(sent[i] >= size || conn_id[i] < 0)
				continue;
			// Write data to the socket
			rv = write(server.fds[conn_id[i]].fd, msg+sent[i], size-sent[i]);
			if(rv < 0) {
				log(ALL, LOG_ERR, "Failed to write to client process %d\n", i);
				conn_id[i] = -1;
				continue;
			}
			sent[i] += rv;
			done = done && sent[i] >= size;
		}
	}
}
