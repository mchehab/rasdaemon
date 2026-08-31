// SPDX-License-Identifier: GPL-2.0-or-later

/*
 * Copyright (C) 2025 Alibaba Inc
 */

#include <errno.h>
#include <pci/pci.h>
#include <linux/pci_regs.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "actions/ras-pcie-edpc.h"
#include "core/modules.h"
#include "core/ras-logger.h"
#include "core/types.h"

#define EDPC_DEVICE "EDPC_DEVICE"

#define PCI_EXP_DPC_CTL_EN_MASK	(PCI_EXP_DPC_CTL_EN_FATAL | \
				 PCI_EXP_DPC_CTL_EN_NONFATAL)

static char *edpc_str(int bit)
{
	if (bit == PCI_EXP_DPC_CTL_EN_FATAL)
		return "Fatal Error";

	if (bit == PCI_EXP_DPC_CTL_EN_NONFATAL)
		return "Non-Fatal Error";

	/*
	 * We can't tell if the error is fatal or not if
	 * both bits are on or off
	 */
	return "Error";
};

struct edpc_device {
	struct pci_dev *dev;
	bool is_cxl_root_port;
	struct edpc_device *next;
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

	cxl_cap = pci_read_word(dev, cap->addr + PCI_CXL_DEV_CAP);
	if (cxl_cap & (PCI_CXL_DEV_CAP_CACHE | PCI_CXL_DEV_CAP_MEM))
		return true;

	return false;
}

/**
 * CXL 2.0 RAS spec: 4.2:
 * Enabling eDPC is not recommended in most CXL 2.0 systems because eDPC
 * containment flow brings the link down, disrupting CXL.cache and
 * CXL.mem traffic which can lead to host timeouts.
 */
static void cxl_check_rp(struct pci_dev *dev, struct edpc_device *dpc)
{
	struct pci_dev *dev_p;
	struct edpc_device *dpc_p;
	for (dev_p = dev->parent; dev_p; dev_p = dev_p->parent) {
		for (dpc_p = dpc->next; dpc_p; dpc_p = dpc_p->next) {
			if (dev_p->domain == dpc_p->dev->domain &&
			    dev_p->bus == dpc_p->dev->bus &&
			    dev_p->dev == dpc_p->dev->dev &&
			    dev_p->func == dpc_p->dev->func) {
				dpc_p->is_cxl_root_port = true;
				log(TERM, LOG_INFO, "Device %x:%x:%x.%x is CXL RP, ignore EDPC config\n",
					dpc_p->dev->domain, dpc_p->dev->bus,
					dpc_p->dev->dev, dpc_p->dev->func);
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

static bool is_downstream_port(struct pci_dev *dev)
{
	struct pci_cap *cap;
	u16 flags;

	pci_fill_info(dev, PCI_FILL_CAPS);
	cap = pci_find_cap(dev, PCI_CAP_ID_EXP, PCI_CAP_NORMAL);
	if (!cap)
		return false;

	flags = pci_read_word(dev, cap->addr + PCI_EXP_FLAGS);
	return ((flags & PCI_EXP_FLAGS_TYPE) >> 4) == PCI_EXP_TYPE_DOWNSTREAM;
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
	    edpc_str(control & PCI_EXP_DPC_CTL_EN_MASK),
	    need_config ? "" : "not");

	if (need_config) {
		control &= ~PCI_EXP_DPC_CTL_EN_MASK;
		control |= PCI_EXP_DPC_CTL_EN_FATAL;
		pci_write_word(dev, cap->addr + PCI_EXP_DPC_CTL, control);
		log(TERM, LOG_INFO, "Device %x:%x:%x.%x EDPC %s and triggered for %s\n",
		    dev->domain, dev->bus, dev->dev, dev->func,
		    (control & PCI_EXP_DPC_CTL_INT_EN) ? "enabled" : "disabled",
		    edpc_str(control & PCI_EXP_DPC_CTL_EN_MASK));
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
	struct pci_dev *dev, *dev_head;
	struct edpc_device *dpc, *tmp;
	int ret = 0, len = 1, i;
	char *pci_names;
	struct pci_filter *filter = NULL;
	struct edpc_device dev_dpc_head = { 0 };

	pacc = pci_alloc();
	if (!pacc)
		return -1;

	pci_init(pacc);
	pci_scan_bus(pacc);

	pci_names = getenv(EDPC_DEVICE);
	if (pci_names && strlen(pci_names) != 0) {
		filter = config_pcie_edpc_device(pacc, pci_names, &len);
		if (!filter) {
			ret = -EINVAL;
			goto free;
		}
	} else {
		len = 0;
	}

	dev_head = pacc->devices;
	for (dev = dev_head; dev; dev = dev->next) {
		pci_fill_info(dev, PCI_FILL_PARENT);
		if (has_edpc(dev) && is_downstream_port(dev)) {
			tmp = calloc(1, sizeof(*tmp));
			if (!tmp) {
				ret = -1;
				goto free;
			}

			tmp->dev = dev;
			tmp->next = dev_dpc_head.next;
			dev_dpc_head.next = tmp;
		}
	}

	for (dev = dev_head; dev; dev = dev->next)
		if (is_cxl_mem_or_cache(dev))
			cxl_check_rp(dev, &dev_dpc_head);

	for (dpc = dev_dpc_head.next; dpc; dpc = dpc->next) {
		if (!dpc->is_cxl_root_port) {
			if (len) {
				for (i = 0; i < len; i++) {
					if (pci_filter_match(&filter[i], dpc->dev)) {
						set_edpc(dpc->dev);
						break;
					}
				}
			} else {
				set_edpc(dpc->dev);
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

static int pcie_edpc_init(struct ras_module_ctx *ctx)
{
	const char *enabled = getenv(PCIE_EDPC_ENABLE);

	if (!enabled || !atoi(enabled))
		return 0;

	return config_pcie_edpc();
}

static const struct ras_module_entry pcie_edpc_module = {
	.name = "pcie-edpc",
	.level = ACTIONS_MODULE,
	.init = pcie_edpc_init,
};

REGISTER_RAS_MODULE(pcie_edpc_module);
