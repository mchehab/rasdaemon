// SPDX-License-Identifier: GPL-2.0-or-later

/*
 * Copyright (C) 2025 Alibaba Inc
 */

#include <pci/pci.h>
#include <linux/pci_regs.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ras-pcie-edpc.h"
#include "ras-logger.h"
#include "types.h"

#define EDPC_DEVICE "EDPC_DEVICE"

#define PCI_EXP_DPC_CTL_EN_MASK	0x3

static char *edpc_str[] = {
	[PCI_EXP_DPC_CTL_EN_FATAL] = "Fatal Error",
	[PCI_EXP_DPC_CTL_EN_NONFATAL] = "Non-Fatal Error",
};

static bool is_cxl_mem_or_cache(struct pci_dev *dev)
{
	struct pci_cap *cap;
	u32 hdr;
	u16 vendor, cxl_cap, id;

	cap = pci_find_cap(dev, PCI_EXT_CAP_ID_DVSEC, PCI_CAP_EXTENDED);
	if (!cap)
		return false;

	hdr = pci_read_long(dev, cap->addr + PCI_DVSEC_HEADER1);
	vendor = hdr & GENMASK(15, 0);
	id = pci_read_word(dev, cap->addr + PCI_DVSEC_HEADER2);
	if (vendor != PCI_DVSEC_VENDOR_ID_CXL || id != PCI_DVSEC_ID_CXL)
		return false;

	cxl_cap = pci_read_word(dev, cap->addr + PCI_CXL_CAP);
	if (cxl_cap & (PCI_CXL_CAP_CACHE | PCI_CXL_CAP_MEM))
		return true;

	return false;
}

/**
 * CXL 2.0 RAS spec: 4.2:
 * Enabling eDPC is not recommended in most CXL 2.0 systems because eDPC
 * containment flow brings the link down, disrupting CXL.cache and
 * CXL.mem traffic which can lead to host timeouts.
 */
static void cxl_check_rp(struct pci_dev *dev, struct pci_dev *dpc)
{
	struct pci_dev *dev_p, *dpc_p;
	for (dev_p = dev->parent; dev_p; dev_p = dev_p->parent) {
		for (dpc_p = dpc->next; dpc_p; dpc_p = dpc_p->next) {
			if (dev_p->domain == dpc_p->domain &&
			    dev_p->bus == dpc_p->bus &&
			    dev_p->dev == dpc_p->dev &&
			    dev_p->func == dpc_p->func) {
				dpc_p->aux = (void *)true;
				log(TERM, LOG_INFO, "Device %x:%x:%x.%x is CXL RP, ignore EDPC config\n",
					dpc_p->domain, dpc_p->bus, dpc_p->dev, dpc_p->func);
			    }
		}
	}
}

static bool has_edpc(struct pci_dev *dev)
{
	struct pci_cap *cap;

	pci_fill_info(dev, PCI_FILL_EXT_CAPS);
	cap = pci_find_cap(dev, PCI_EXT_CAP_ID_DPC, PCI_CAP_EXTENDED);
	if (!cap)
		return false;
	return true;
}

static void set_edpc(struct pci_dev *dev)
{
	struct pci_cap *cap;
	u16 control;
	int need_config = 0;

	cap = pci_find_cap(dev, PCI_EXT_CAP_ID_DPC, PCI_CAP_EXTENDED);
	if (!cap)
		return;

	control = pci_read_word(dev, cap->addr + PCI_EXP_DPC_CTL);
	need_config = PCI_DPC_CTL_TRIGGER(control) == PCI_EXP_DPC_CTL_EN_FATAL ? 0 : 1;
	log(TERM, LOG_INFO, "Device %x:%x:%x.%x origin EDPC %s and triggered for %s, %s need config\n",
	    dev->domain, dev->bus, dev->dev, dev->func,
	    (control & PCI_EXP_DPC_CTL_INT_EN) ? "enabled" : "disabled",
	    edpc_str[control & PCI_EXP_DPC_CTL_EN_MASK],
	    need_config ? "" : "not");

	if (need_config) {
		control &= PCI_EXP_DPC_CTL_EN_MASK;
		control |= PCI_EXP_DPC_CTL_EN_FATAL;
		pci_write_word(dev, cap->addr + PCI_EXP_DPC_CTL, control);
		log(TERM, LOG_INFO, "Device %x:%x:%x.%x EDPC %s and triggered for %s\n",
		    dev->domain, dev->bus, dev->dev, dev->func,
		    (control & PCI_EXP_DPC_CTL_INT_EN) ? "enabled" : "disabled",
		    edpc_str[control & PCI_EXP_DPC_CTL_EN_MASK]);
	}
}

static struct pci_filter *config_pcie_edpc_device(struct pci_access *pacc, char *names, int *len)
{
	int i;
	struct pci_filter *filter = NULL;
	char *token, *err, pci_names[MAX_PATH + 1];

	strscpy(pci_names, names, sizeof(pci_names));
	for (i = 0; pci_names[i] != '\0'; i++)
		if (pci_names[i] == ',')
			(*len)++;

	filter = calloc(*len, sizeof(struct pci_filter));
	if (!filter)
		return NULL;

	i = 0;
	token = strtok(pci_names, ",");
	while (token) {
		pci_filter_init(pacc, &filter[i]);
		err = pci_filter_parse_slot(&filter[i++], token);
		if (err) {
			free(filter);
			log(TERM, LOG_ERR, "Invalid PCI device name %s\n", err);
			return NULL;
		}
		token = strtok(NULL, ",");
	}

	log(TERM, LOG_ERR, "Config PCIE EDPC for: %s\n", names);

	return filter;
}

int config_pcie_edpc(void)
{
	struct pci_access *pacc;
	struct pci_dev *dev, *dev_head, *tmp;
	int ret = 0, len = 1, i;
	char *pci_names;
	struct pci_filter *filter = NULL;
	struct pci_dev dev_dpc_head = { 0 };

	pacc = pci_alloc();
	if (!pacc)
		return -1;

	pci_init(pacc);
	pci_scan_bus(pacc);

	pci_names = getenv(EDPC_DEVICE);
	if (pci_names && strlen(pci_names) != 0) {
		filter = config_pcie_edpc_device(pacc, pci_names, &len);
		if (!filter)
			goto free;
	} else {
		len = 0;
	}

	dev_head = pacc->devices;
	for (dev = dev_head; dev; dev = dev->next) {
		pci_fill_info(dev, PCI_FILL_PARENT);
		if (has_edpc(dev)) {
			tmp = malloc(sizeof(struct pci_dev));
			if (!tmp) {
				ret = -1;
				goto free;
			}

			memcpy(tmp, dev, sizeof(struct pci_dev));
			tmp->next = dev_dpc_head.next;
			dev_dpc_head.next = tmp;
		}
	}

	for (dev = dev_head; dev; dev = dev->next)
		if (is_cxl_mem_or_cache(dev))
			cxl_check_rp(dev, &dev_dpc_head);

	for (dev = dev_dpc_head.next; dev; dev = dev->next) {
		if (!dev->aux) {
			if (len) {
				for (i = 0; i < len; i++) {
					if (pci_filter_match(&filter[i], dev)) {
						set_edpc(dev);
						break;
					}
				}
			} else {
				set_edpc(dev);
			}
		}
	}

free:
	while (dev_dpc_head.next) {
		tmp = dev_dpc_head.next;
		dev_dpc_head.next = tmp->next;
		free(tmp);
	}

	pci_cleanup(pacc);
	free(filter);
	return ret;
}
