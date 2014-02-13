#ifndef __RAS_REPORT_H
#define __RAS_REPORT_H

#include "ras-record.h"
#include "ras-events.h"
#include "ras-mc-handler.h"
#include "ras-mce-handler.h"
#include "ras-aer-handler.h"

/* Maximal length of backtrace. */
#define MAX_BACKTRACE_SIZE (1024*1024)
/* Amount of data received from one client for a message before reporting error. */
#define MAX_MESSAGE_SIZE (4*MAX_BACKTRACE_SIZE)
/* Maximal number of characters read from socket at once. */
#define INPUT_BUFFER_SIZE (8*1024)
/* ABRT socket file */
#define ABRT_SOCKET "/var/run/abrt/abrt.socket"

enum {
	MC_EVENT,
	MCE_EVENT,
	AER_EVENT
};

#ifdef HAVE_ABRT_REPORT

int ras_report_mc_event(struct ras_events *ras, struct ras_mc_event *ev);
int ras_report_aer_event(struct ras_events *ras, struct ras_aer_event *ev);
int ras_report_mce_event(struct ras_events *ras, struct mce_event *ev);

#else

static inline int ras_report_mc_event(struct ras_events *ras, struct ras_mc_event *ev) { return 0; };
static inline int ras_report_aer_event(struct ras_events *ras, struct ras_aer_event *ev) { return 0; };
static inline int ras_report_mce_event(struct ras_events *ras, struct mce_event *ev) { return 0; };

#endif

#endif
