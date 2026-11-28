// SPDX-License-Identifier: GPL-2.0-only

/*
 * PCI hotplug and PCIe link change events.
 *
 * The kernel reports hotplug link and presence changes with pci:pci_hp_event
 * and each link speed update with pci:pcie_link_event. A link that comes back
 * at a lower speed or width, or that goes down without a removal request, can
 * point to a failing device, connector or cable.
 */

#include <errno.h>
#include <linux/pci_regs.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "core/modules.h"
#include "core/ras-logger.h"
#include "core/types.h"
#include "db/ras-db.h"
#include "events/ras-pcie-link-handler.h"

#define PCI_TRACE_GROUP		"pci"
#define PCI_HP_TRACE_EVENT	"pci_hp_event"
#define PCIE_LINK_TRACE_EVENT	"pcie_link_event"

/*
 * enum pci_bus_speed values (include/linux/pci.h). The kernel does not export
 * them; trace events carry the raw value.
 */
#define PCIE_SPEED_2_5GT	0x14
#define PCI_SPEED_UNKNOWN	0xff

static int ras_pci_hp_event_handler(struct trace_seq *s,
				    struct tep_record *record,
				    struct tep_event *event, void *context);
static int ras_pcie_link_event_handler(struct trace_seq *s,
				       struct tep_record *record,
				       struct tep_event *event, void *context);
static int ras_pci_hp_prepare(struct ras_events *ras);
static int ras_pcie_link_prepare(struct ras_events *ras);
static int db_pci_hp_event(struct ras_events *ras, void *priv);
static int db_pcie_link_event(struct ras_events *ras, void *priv);

#ifdef HAVE_UNITTEST
int test_pcie_link(void) __attribute__((weak));
#endif

/* The unit test, attached to the link event, covers both handlers. */
static const struct ras_event_entry ras_pci_hp_event = {
	.group = PCI_TRACE_GROUP, .event = PCI_HP_TRACE_EVENT,
	.handler = ras_pci_hp_event_handler, .id = PCI_HP_EVENT,
	.prepare = ras_pci_hp_prepare,
	.record = db_pci_hp_event,
};

REGISTER_RAS_EVENT(ras_pci_hp_event);

static const struct ras_event_entry ras_pcie_link_event = {
	.group = PCI_TRACE_GROUP, .event = PCIE_LINK_TRACE_EVENT,
	.handler = ras_pcie_link_event_handler, .id = PCIE_LINK_EVENT,
	.prepare = ras_pcie_link_prepare,
#ifdef HAVE_UNITTEST
	.test_group = TEST_GROUP_EVENTS, .test = test_pcie_link,
#endif
	.record = db_pcie_link_event,
};

REGISTER_RAS_EVENT(ras_pcie_link_event);

/* enum pci_hotplug_event (include/uapi/linux/pci.h) */
static const char * const pci_hp_events[] = {
	"LINK_UP",
	"LINK_DOWN",
	"CARD_PRESENT",
	"CARD_NOT_PRESENT",
};

/* enum pcie_link_change_reason (drivers/pci/pci.h) */
static const char * const pcie_link_reasons[] = {
	"Link retrain",
	"Bus enumeration",
	"Bandwidth notification enable",
	"Bandwidth notification IRQ",
	"Hotplug event",
};

static const char * const pcie_port_types[] = {
	[PCI_EXP_TYPE_ENDPOINT]		= "Endpoint",
	[PCI_EXP_TYPE_LEG_END]		= "Legacy Endpoint",
	[PCI_EXP_TYPE_ROOT_PORT]	= "Root Port",
	[PCI_EXP_TYPE_UPSTREAM]		= "Upstream Port",
	[PCI_EXP_TYPE_DOWNSTREAM]	= "Downstream Port",
	[PCI_EXP_TYPE_PCI_BRIDGE]	= "PCIe to PCI/PCI-X Bridge",
	[PCI_EXP_TYPE_PCIE_BRIDGE]	= "PCI/PCI-X to PCIe Bridge",
	[PCI_EXP_TYPE_RC_END]		= "Root Complex Integrated Endpoint",
	[PCI_EXP_TYPE_RC_EC]		= "Root Complex Event Collector",
};

/* Indexed by enum pci_bus_speed minus PCIE_SPEED_2_5GT */
static const char * const pcie_speeds[] = {
	"2.5 GT/s",
	"5.0 GT/s",
	"8.0 GT/s",
	"16.0 GT/s",
	"32.0 GT/s",
	"64.0 GT/s",
};

/* The Link Status bits the kernel copies into link_status */
static const struct {
	unsigned int bit;
	const char *name;
} pcie_link_status_flags[] = {
	{ PCI_EXP_LNKSTA_LT,	"LT" },
	{ PCI_EXP_LNKSTA_DLLLA,	"DLLLA" },
	{ PCI_EXP_LNKSTA_LBMS,	"LBMS" },
	{ PCI_EXP_LNKSTA_LABS,	"LABS" },
};

static void pcie_link_name(char *buf, size_t size, const char * const *names,
			   size_t count, long long value)
{
	if (value >= 0 && (unsigned long long)value < count && names[value])
		strscpy(buf, names[value], size);
	else
		snprintf(buf, size, "unknown (%lld)", value);
}

static void pcie_link_speed(char *buf, size_t size, unsigned int speed)
{
	if (speed >= PCIE_SPEED_2_5GT &&
	    speed - PCIE_SPEED_2_5GT < ARRAY_SIZE(pcie_speeds))
		strscpy(buf, pcie_speeds[speed - PCIE_SPEED_2_5GT], size);
	else if (speed == PCI_SPEED_UNKNOWN)
		strscpy(buf, "unknown", size);
	else
		snprintf(buf, size, "unknown (%u)", speed);
}

static void pcie_link_status(char *buf, size_t size, unsigned int status)
{
	size_t i, len = 0;
	int rc;

	buf[0] = '\0';
	for (i = 0; i < ARRAY_SIZE(pcie_link_status_flags); i++) {
		if (!(status & pcie_link_status_flags[i].bit))
			continue;
		rc = snprintf(buf + len, size - len, "%s%s", len ? "|" : "",
			      pcie_link_status_flags[i].name);
		if (rc < 0 || (size_t)rc >= size - len)
			break;
		len += rc;
	}
}

static void pcie_link_timestamp(struct ras_events *ras,
				struct tep_record *record,
				char *buf, size_t size)
{
	time_t now;
	struct tm *tm;

	if (ras->use_uptime)
		now = record->ts / user_hz + ras->uptime_diff;
	else
		now = time(NULL);

	buf[0] = '\0';
	tm = localtime(&now);
	if (tm)
		strftime(buf, size, "%Y-%m-%d %H:%M:%S %z", tm);
}

/*
 * Copy a __string() field. The kernel stores the terminating NUL, but the
 * copy is bounded by the field length in case the record is damaged.
 */
static int pcie_link_get_str(struct trace_seq *s, struct tep_event *event,
			     struct tep_record *record, const char *name,
			     char *buf, size_t size)
{
	const char *raw;
	size_t len;
	int field_len;

	raw = tep_get_field_raw(s, event, name, record, &field_len, 1);
	if (!raw || field_len < 0)
		return -1;

	len = strnlen(raw, field_len);
	if (len >= size)
		len = size - 1;
	memcpy(buf, raw, len);
	buf[len] = '\0';

	return 0;
}

static int ras_pci_hp_event_handler(struct trace_seq *s,
				    struct tep_record *record,
				    struct tep_event *event, void *context)
{
	struct ras_events *ras = context;
	struct ras_pci_hp_event ev;
	unsigned long long val;

	pcie_link_timestamp(ras, record, ev.timestamp, sizeof(ev.timestamp));

	if (pcie_link_get_str(s, event, record, "port_name", ev.port_name,
			      sizeof(ev.port_name)) < 0)
		return -1;
	if (pcie_link_get_str(s, event, record, "slot", ev.slot,
			      sizeof(ev.slot)) < 0)
		return -1;
	if (tep_get_field_val(s, event, "event", record, &val, 1) < 0)
		return -1;
	pcie_link_name(ev.event, sizeof(ev.event), pci_hp_events,
		       ARRAY_SIZE(pci_hp_events), (int)val);

	trace_seq_printf(s, "%s %s port %s, slot %s: %s",
			 loglevel_str[LOGLEVEL_NOTICE], ev.timestamp,
			 ev.port_name, ev.slot, ev.event);

	ras_event_publish(ras, PCI_HP_EVENT, &ev);

	return 0;
}

static int ras_pcie_link_event_handler(struct trace_seq *s,
				       struct tep_record *record,
				       struct tep_event *event, void *context)
{
	struct ras_events *ras = context;
	struct ras_pcie_link_event ev;
	unsigned long long val;

	pcie_link_timestamp(ras, record, ev.timestamp, sizeof(ev.timestamp));

	if (pcie_link_get_str(s, event, record, "port_name", ev.port_name,
			      sizeof(ev.port_name)) < 0)
		return -1;

	if (tep_get_field_val(s, event, "type", record, &val, 1) < 0)
		return -1;
	pcie_link_name(ev.port_type, sizeof(ev.port_type), pcie_port_types,
		       ARRAY_SIZE(pcie_port_types), (unsigned int)val);

	if (tep_get_field_val(s, event, "reason", record, &val, 1) < 0)
		return -1;
	pcie_link_name(ev.reason, sizeof(ev.reason), pcie_link_reasons,
		       ARRAY_SIZE(pcie_link_reasons), (unsigned int)val);

	if (tep_get_field_val(s, event, "cur_bus_speed", record, &val, 1) < 0)
		return -1;
	pcie_link_speed(ev.cur_speed, sizeof(ev.cur_speed), (unsigned int)val);

	if (tep_get_field_val(s, event, "max_bus_speed", record, &val, 1) < 0)
		return -1;
	pcie_link_speed(ev.max_speed, sizeof(ev.max_speed), (unsigned int)val);

	if (tep_get_field_val(s, event, "width", record, &val, 1) < 0)
		return -1;
	ev.width = (unsigned int)val;

	if (tep_get_field_val(s, event, "flit_mode", record, &val, 1) < 0)
		return -1;
	ev.flit_mode = (unsigned int)val;

	if (tep_get_field_val(s, event, "link_status", record, &val, 1) < 0)
		return -1;
	pcie_link_status(ev.link_status, sizeof(ev.link_status),
			 (unsigned int)val);

	trace_seq_printf(s,
			 "%s %s port %s (%s): %s, speed %s (bus max %s), width x%u, flit mode %u, status %s",
			 loglevel_str[LOGLEVEL_NOTICE], ev.timestamp,
			 ev.port_name, ev.port_type, ev.reason, ev.cur_speed,
			 ev.max_speed, ev.width, ev.flit_mode,
			 ev.link_status[0] ? ev.link_status : "none");

	ras_event_publish(ras, PCIE_LINK_EVENT, &ev);

	return 0;
}

/* Older kernels lack these events: skip them without reporting an error. */
static int pcie_link_prepare(struct ras_events *ras, const char *event)
{
	if (ras_event_exists(ras, PCI_TRACE_GROUP, event))
		return 0;

	log(TERM, LOG_INFO, "%s:%s not available on this kernel\n",
	    PCI_TRACE_GROUP, event);
	return -ENOENT;
}

static int ras_pci_hp_prepare(struct ras_events *ras)
{
	return pcie_link_prepare(ras, PCI_HP_TRACE_EVENT);
}

static int ras_pcie_link_prepare(struct ras_events *ras)
{
	return pcie_link_prepare(ras, PCIE_LINK_TRACE_EVENT);
}

static const struct db_fields pci_hp_event_fields[] = {
	{ .name = "id",		.type = DB_TYPE_SERIAL, .is_pk = true },
	{ .name = "timestamp",	.type = DB_TYPE_TIMESTAMP, .create_index = true },
	{ .name = "port_name",	.type = DB_TYPE_TEXT },
	{ .name = "slot",	.type = DB_TYPE_TEXT },
	{ .name = "event",	.type = DB_TYPE_TEXT },
};

static const struct db_table_descriptor pci_hp_event_tab = {
	.name = "pci_hp_event",
	.fields = pci_hp_event_fields,
	.num_fields = ARRAY_SIZE(pci_hp_event_fields),
};

static struct db_desc_and_stmt pci_hp_event_db = {
	.desc = &pci_hp_event_tab,
};

static const struct db_fields pcie_link_event_fields[] = {
	{ .name = "id",		 .type = DB_TYPE_SERIAL, .is_pk = true },
	{ .name = "timestamp",	 .type = DB_TYPE_TIMESTAMP, .create_index = true },
	{ .name = "port_name",	 .type = DB_TYPE_TEXT },
	{ .name = "port_type",	 .type = DB_TYPE_TEXT },
	{ .name = "reason",	 .type = DB_TYPE_TEXT },
	{ .name = "cur_speed",	 .type = DB_TYPE_TEXT },
	{ .name = "max_speed",	 .type = DB_TYPE_TEXT },
	{ .name = "width",	 .type = DB_TYPE_INT32 },
	{ .name = "flit_mode",	 .type = DB_TYPE_INT32 },
	{ .name = "link_status", .type = DB_TYPE_TEXT },
};

static const struct db_table_descriptor pcie_link_event_tab = {
	.name = "pcie_link_event",
	.fields = pcie_link_event_fields,
	.num_fields = ARRAY_SIZE(pcie_link_event_fields),
};

static struct db_desc_and_stmt pcie_link_event_db = {
	.desc = &pcie_link_event_tab,
};

static int db_pci_hp_event(struct ras_events *ras, void *priv)
{
	struct ras_pci_hp_event *ev = priv;
	int rc, pos = 1;

	if (!pci_hp_event_db.stmt)
		return 0;

	db_bind(&pci_hp_event_tab, pci_hp_event_db.stmt, pos++,
		(uint64_t)ev->timestamp, -1);
	db_bind(&pci_hp_event_tab, pci_hp_event_db.stmt, pos++,
		(uint64_t)ev->port_name, -1);
	db_bind(&pci_hp_event_tab, pci_hp_event_db.stmt, pos++,
		(uint64_t)ev->slot, -1);
	db_bind(&pci_hp_event_tab, pci_hp_event_db.stmt, pos++,
		(uint64_t)ev->event, -1);

	rc = db_eval_stmt(pci_hp_event_db.stmt, "pci_hp_event");
	if (!rc)
		log(TERM, LOG_INFO, "register inserted at db\n");

	return rc;
}

static int db_pcie_link_event(struct ras_events *ras, void *priv)
{
	struct ras_pcie_link_event *ev = priv;
	int rc, pos = 1;

	if (!pcie_link_event_db.stmt)
		return 0;

	db_bind(&pcie_link_event_tab, pcie_link_event_db.stmt, pos++,
		(uint64_t)ev->timestamp, -1);
	db_bind(&pcie_link_event_tab, pcie_link_event_db.stmt, pos++,
		(uint64_t)ev->port_name, -1);
	db_bind(&pcie_link_event_tab, pcie_link_event_db.stmt, pos++,
		(uint64_t)ev->port_type, -1);
	db_bind(&pcie_link_event_tab, pcie_link_event_db.stmt, pos++,
		(uint64_t)ev->reason, -1);
	db_bind(&pcie_link_event_tab, pcie_link_event_db.stmt, pos++,
		(uint64_t)ev->cur_speed, -1);
	db_bind(&pcie_link_event_tab, pcie_link_event_db.stmt, pos++,
		(uint64_t)ev->max_speed, -1);
	db_bind(&pcie_link_event_tab, pcie_link_event_db.stmt, pos++,
		ev->width, -1);
	db_bind(&pcie_link_event_tab, pcie_link_event_db.stmt, pos++,
		ev->flit_mode, -1);
	db_bind(&pcie_link_event_tab, pcie_link_event_db.stmt, pos++,
		(uint64_t)ev->link_status, -1);

	rc = db_eval_stmt(pcie_link_event_db.stmt, "pcie_link_event");
	if (!rc)
		log(TERM, LOG_INFO, "register inserted at db\n");

	return rc;
}

static int ras_pcie_link_init(struct ras_module_ctx *ctx)
{
	int rc;

	rc = ras_db_table_register(ctx, &pci_hp_event_db);
	if (rc)
		return rc;

	rc = ras_db_table_register(ctx, &pcie_link_event_db);
	if (rc)
		ras_db_table_unregister(ctx);

	return rc;
}

static void ras_pcie_link_cleanup(struct ras_module_ctx *ctx)
{
	ras_db_table_unregister(ctx);
}

static const struct ras_module_entry ras_pcie_link_module = {
	.name = "pcie-link-event",
	.level = BASE_EVENT_MODULE,
	.init = ras_pcie_link_init,
	.cleanup = ras_pcie_link_cleanup,
};

REGISTER_RAS_MODULE(ras_pcie_link_module);
