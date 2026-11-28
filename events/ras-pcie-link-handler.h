/* SPDX-License-Identifier: GPL-2.0-only */

/*
 * PCI hotplug and PCIe link change events.
 */

#ifndef __RAS_PCIE_LINK_HANDLER_H
#define __RAS_PCIE_LINK_HANDLER_H

#include "core/ras-events.h"

/*
 * Formats of the kernel trace events (include/trace/events/pci.h):
 *
 * pci:pci_hp_event
 *	field:__data_loc char[] port_name;	offset:8;	size:4;	signed:0;
 *	field:__data_loc char[] slot;	offset:12;	size:4;	signed:0;
 *	field:int event;	offset:16;	size:4;	signed:1;
 *
 * pci:pcie_link_event
 *	field:__data_loc char[] port_name;	offset:8;	size:4;	signed:0;
 *	field:unsigned int type;	offset:12;	size:4;	signed:0;
 *	field:unsigned int reason;	offset:16;	size:4;	signed:0;
 *	field:unsigned int cur_bus_speed;	offset:20;	size:4;	signed:0;
 *	field:unsigned int max_bus_speed;	offset:24;	size:4;	signed:0;
 *	field:unsigned int width;	offset:28;	size:4;	signed:0;
 *	field:unsigned int flit_mode;	offset:32;	size:4;	signed:0;
 *	field:unsigned int link_status;	offset:36;	size:4;	signed:0;
 */

/* PCI address of the port, such as "0000:00:1c.0", or a hotplug slot name */
#define PCIE_LINK_NAME_LEN	64
/* Decoded enumeration value, such as "Bandwidth notification IRQ" */
#define PCIE_LINK_DESC_LEN	40

/**
 * struct ras_pci_hp_event - decoded pci:pci_hp_event payload
 * @timestamp: event time
 * @port_name: PCI address of the hotplug port
 * @slot: hotplug slot name
 * @event: link up or down, or card present or removed
 */
struct ras_pci_hp_event {
	char timestamp[64];
	char port_name[PCIE_LINK_NAME_LEN];
	char slot[PCIE_LINK_NAME_LEN];
	char event[PCIE_LINK_DESC_LEN];
};

/**
 * struct ras_pcie_link_event - decoded pci:pcie_link_event payload
 * @timestamp: event time
 * @port_name: PCI address of the port above the link
 * @port_type: PCIe device/port type of that port
 * @reason: why the kernel updated the link speed
 * @cur_speed: current link speed
 * @max_speed: maximum speed of the bus
 * @width: negotiated link width, in lanes
 * @flit_mode: link operates in Flit mode
 * @link_status: Link Status register flags (LT, DLLLA, LBMS, LABS)
 */
struct ras_pcie_link_event {
	char timestamp[64];
	char port_name[PCIE_LINK_NAME_LEN];
	char port_type[PCIE_LINK_DESC_LEN];
	char reason[PCIE_LINK_DESC_LEN];
	char cur_speed[PCIE_LINK_DESC_LEN];
	char max_speed[PCIE_LINK_DESC_LEN];
	unsigned int width;
	unsigned int flit_mode;
	char link_status[PCIE_LINK_DESC_LEN];
};

#endif
