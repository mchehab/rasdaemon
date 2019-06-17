/*
 * Copyright (c) 2017 Hisilicon Limited.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ras-record.h"
#include "ras-logger.h"
#include "ras-report.h"
#include "ras-non-standard-handler.h"

/* common definitions */

/* HISI SAS definitions */
#define HISI_SAS_VALID_PA             BIT(0)
#define HISI_SAS_VALID_MB_ERR         BIT(1)
#define HISI_SAS_VALID_ERR_TYPE       BIT(2)
#define HISI_SAS_VALID_AXI_ERR_INFO   BIT(3)

struct hisi_sas_err_sec {
	uint64_t   val_bits;
	uint64_t   physical_addr;
	uint32_t   mb;
	uint32_t   type;
	uint32_t   axi_err_info;
};

/* Common Functions */
static char *err_bit_type(int etype)
{
	switch (etype) {
	case 0x0: return "single-bit ecc";
	case 0x1: return "multi-bit ecc";
	}
	return "unknown error";
}

/* SAS Functions */
static char *sas_err_type(int etype)
{
	switch (etype) {
	case 0x0001: return "hgc_dqe ecc";
	case 0x0002: return "hgc_iost ecc";
	case 0x0004: return "hgc_itct ecc";
	case 0x0008: return "hgc_iostl ecc";
	case 0x0010: return "hgc_itctl ecc";
	case 0x0020: return "hgc_cqe ecc";
	case 0x0040: return "rxm_mem0 ecc";
	case 0x0080: return "rxm_mem1 ecc";
	case 0x0100: return "rxm_mem2 ecc";
	case 0x0200: return "rxm_mem3 ecc";
	case 0x0400: return "wp_depth";
	case 0x0800: return "iptt_slot_no_match";
	case 0x1000: return "rp_depth";
	case 0x2000: return "axi err";
	case 0x4000: return "fifo err";
	case 0x8000: return "lm_add_fetch_list";
	case 0x10000: return "hgc_abt_fetch_lm";
	}
	return "unknown error";
}

static char *sas_axi_err_type(int etype)
{
	switch (etype) {
	case 0x0001: return "IOST_AXI_W_ERR";
	case 0x0002: return "IOST_AXI_R_ERR";
	case 0x0004: return "ITCT_AXI_W_ERR";
	case 0x0008: return "ITCT_AXI_R_ERR";
	case 0x0010: return "SATA_AXI_W_ERR";
	case 0x0020: return "SATA_AXI_R_ERR";
	case 0x0040: return "DQE_AXI_R_ERR";
	case 0x0080: return "CQE_AXI_W_ERR";
	case 0x0100: return "CQE_WINFO_FIFO";
	case 0x0200: return "CQE_MSG_FIFIO";
	case 0x0400: return "GETDQE_FIFO";
	case 0x0800: return "CMDP_FIFO";
	case 0x1000: return "AWTCTRL_FIFO";
	}
	return "unknown error";
}

static int decode_hip07_sas_error(struct ras_events *ras,
				  struct ras_ns_dec_tab *dec_tab,
				  struct trace_seq *s, const void *error)
{
	char buf[1024];
	char *p = buf;
	const struct hisi_sas_err_sec *err = error;

	if (err->val_bits == 0) {
		trace_seq_printf(s, "%s: no valid error data\n",
				 __func__);
		return -1;
	}
	p += sprintf(p, "[");
	if (err->val_bits & HISI_SAS_VALID_PA)
		p += sprintf(p, "phy addr = 0x%p: ",
			     (void *)err->physical_addr);

	if (err->val_bits & HISI_SAS_VALID_MB_ERR)
		p += sprintf(p, "%s: ", err_bit_type(err->mb));

	if (err->val_bits & HISI_SAS_VALID_ERR_TYPE)
		p += sprintf(p, "error type = %s: ",
			     sas_err_type(err->type));

	if (err->val_bits & HISI_SAS_VALID_AXI_ERR_INFO)
		p += sprintf(p, "axi error type = %s",
			     sas_axi_err_type(err->axi_err_info));

	p += sprintf(p, "]");

	trace_seq_printf(s, "\nHISI HIP07: SAS error: %s\n", buf);
	return 0;
}

static int decode_hip07_hns_error(struct ras_events *ras,
				  struct ras_ns_dec_tab *dec_tab,
				  struct trace_seq *s, const void *error)
{
	return 0;
}

struct ras_ns_dec_tab hisi_ns_dec_tab[] = {
	{
		.sec_type = "daffd8146eba4d8c8a91bc9bbf4aa301",
		.decode = decode_hip07_sas_error,
	},
	{
		.sec_type = "fbc2d923ea7a453dab132949f5af9e53",
		.decode = decode_hip07_hns_error,
	},
	{ /* sentinel */ }
};

__attribute__((constructor))
static void hip07_init(void)
{
	register_ns_dec_tab(hisi_ns_dec_tab);
}
