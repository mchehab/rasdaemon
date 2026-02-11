// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2026, Zhaoxin, Inc. All rights reserved.
 * Author: Lyle Li <LyleLi-oc@zhaoxin.com>
 */

#include <stdio.h>
#include <string.h>

#include "bitfield.h"
#include "ras-mce-handler.h"

/* Zhaoxin KH-50000 CPU Error Bank */
#define MCE_CPU_BANK		0x0
#define MCE_PL2_CACHE_BANK	0x1
#define MCE_CRMCA0_PCIE_BANK	0x2
#define MCE_CRMCA0_IOD_ZDI_BANK 0x3
#define MCE_SVID0_BANK		0x4
#define MCE_CRMCA1_IOD_ZDI_BANK	0x5
#define MCE_CRMCA1_PCIE_BANK	0x6
#define MCE_CCD_ZDI_BANK	0x7
#define MCE_DRAM_BANK0		0x9
#define MCE_DRAM_BANK1		0xA
#define MCE_DRAM_BANK2		0xB
#define MCE_DRAM_BANK3		0xC
#define MCE_DRAM_BANK4		0xD
#define MCE_DRAM_BANK5		0xE
#define MCE_DRAM_BANK6		0xF
#define MCE_DRAM_BANK7		0x10
#define MCE_LLC_CACHE_BANK	0x11
#define MCE_DRAM_BANK8		0x12
#define MCE_DRAM_BANK9		0x13
#define MCE_DRAM_BANK10		0x14
#define MCE_DRAM_BANK11		0x15
#define MCE_SVID2_BANK		0x16
#define MCE_ZPI_BANK		0x1A
#define MCE_SVID1_BANK		0x1B
#define MCE_HIF_H0_BANK		0x1C
#define MCE_HIF_H1_BANK		0x1D
#define MCE_HIF_H2_BANK		0x1E
#define MCE_HIF_H3_BANK		0x1F

#define IOD_ZDI_DEV_COUNT	6
#define ZPI_DEV_COUNT		3

static char *bank_name_str[] = {
	"CPU Error Bank",
	"PL2 Cache Error Bank",
	"CRMCA0 PCIE Error Bank",
	"CRMCA0 IOD ZDI Error Bank",
	"SVID0 Error Bank",
	"CRMCA1 IOD ZDI Error Bank",
	"CRMCA1 PCIE Error Bank",
	"CCD ZDI Error Bank",
	"Reserved Error Bank",
	"Memory Error Bank",
	"Memory Error Bank",
	"Memory Error Bank",
	"Memory Error Bank",
	"Memory Error Bank",
	"Memory Error Bank",
	"Memory Error Bank",
	"Memory Error Bank",
	"LLC Cache Error Bank",
	"Memory Error Bank",
	"Memory Error Bank",
	"Memory Error Bank",
	"Memory Error Bank",
	"SVID2 Error Bank",
	"Reserved Error Bank",
	"Reserved Error Bank",
	"Reserved Error Bank",
	"ZPI Error Bank",
	"SVID1 Error Bank",
	"HIF H0 Error Bank",
	"HIF H1 Error Bank",
	"HIF H2 Error Bank",
	"HIF H3 Error Bank",
};

static char *cpu_error_str[] = {
	"Unknown error",
	"Unknown error",
	"Machine hung error",
	"Undefined ucode address error"
};

static char *cache_error_str[] = {
	"Unknown Error",
	"ECC single bit error for data part in the same line",
	"ECC single bit error for different line",
	"ECC multi bit error for data part"
};

static char *pcie_error_str[] = {
	"Fatal error",
	"Non-fatal error",
	"Correctable error",
};

static char *iod_zdi_zpi_error_str[] = {
	"Unknown error",
	"Receiver overflow status error(TL)",
	"Flow control protocol error(TL)",
	"Surprise down error",
	"Data link protocol error(DLL)",
	"Replay timer timeout error(DLL)",
	"REPLAY_NUM Rollover error(DLL)",
	"Bad data link layer packet error(DLL)",
	"Bad transaction layer packet error(DLL)",
	"Receiver error(PHY)",
	"Phy raining error(PHY)",
	"Link-width down-mode due to link unreliable",
	"Unknown error",
	"Unknown error",
	"Link-speed down-mode due to link unreliable",
	"Unknown error",
	"X32X24 Link-width down-mode due to link unreliable",
	"X16X12 Link-width down-mode due to link unreliable",
	"X8 Link-width down-mode due to link unreliable",
	"X4 Link-width down-mode due to link unreliable",
	"X2 Link-width down-mode due to link unreliable",
	"GEN4 Link-speed down-mode due to link unreliable",
	"GEN3 Link-speed down-mode due to link unreliable",
	"GEN2 Link-speed down-mode due to link unreliable",
};

static char *ccd_zdi_error_str[] = {
	"Unknown error",
	"Receive overflow error",
	"PHY training error",
	"FC protocol error",
	"Surprise down error",
	"DLLM protocol error",
	"DLLM replay timeout error",
	"DLLM replay number rollover report",
	"Bad DLLP error",
	"Bad TLP error",
	"Gen2 unreliable error",
	"Gen3 unreliable error",
	"Gen4 unreliable error",
	"X2 unreliable error",
	"X4 unreliable error",
	"X8 unreliable error",
	"X16/X12 unreliable error",
	"X32/X24 unreliable error",
};

static char *svid_error_str[] = {
	"No error",
	"SVID Resend fail error",
	"VRM Over current error",
	"VRM Over temp error",
	"VRM Parity error",
};

static char *mem_error_mccod_str[] = {
	"Generic undefined request error",
	"Memory read error",
	"Memory write error",
	"Address/Command error",
	"Memory scrubbing error",
	"data posion enable, Error source is dramc, maser normal read",
	"data posion enable, Error source is dramc, patrol read",
	"key hit error",
};

static char *mem_channel_str[] = {
	"channel A0",
	"channel B0",
	"channel A1",
	"channel B1",
	"channel C0",
	"channel C1",
	"channel A2",
	"channel B2",
	"channel C2",
	"channel A3",
	"channel B3",
	"channel C3",
};

static char *mem_specific_error_str[] = {
	"Unknown error",
	"Single bit ECC error",
	"Multiple bit ECC error",
	"Command parity error",
	"CRC error",
	"Parity error retry failed",
	"CRC error retry failed",
	"CPUIF CHA0 DVAD decode error(reserved)",
	"CPUIF CHB0 DVAD decode error(reserved)",
	"CPUIF CHA1 DVAD decode error(reserved)",
	"CPUIF CHB1 DVAD decode error(reserved)",
	"MCUTRF DVAD decode error(reserved)",
	"GMINT CHA0 DVAD decode error(reserved)",
	"GMINT CHB0 DVAD decode error(reserved)",
	"GMINT CHA1 DVAD decode error(reserved)",
	"GMINT CHB1 DVAD decode error(reserved)",
	"Key not hit error",
};

static char *hif_error_str[] = {
	"Unknown error",
	"HIF dvad error",
	"SNT multi bit ecc error",
	"SNT single bit ecc error",
	"CXL decpoison uc error",
	"CXL decpoison ce error",
	"CXL parity error",
};

static inline void bank_name(struct mce_event *e)
{
	mce_snprintf(e->bank_name, "%s", bank_name_str[e->bank]);
}

static void decode_cpu_error(struct mce_event *e)
{
	uint32_t core_id = EXTRACT(e->status, 16, 18);
	uint32_t ccd_id = EXTRACT(e->status, 19, 22);
	uint32_t siod_id = EXTRACT(e->status, 23, 24);
	uint32_t err_number = EXTRACT(e->status, 25, 29);

	mce_snprintf(e->mcastatus_msg, "siod_id %d ccd_id %d core_id %d occurred %s",
		     siod_id, ccd_id, core_id,
		     err_number < ARRAY_SIZE(cpu_error_str) ?
		     cpu_error_str[err_number] : "Unknown error");
}

static void decode_cache_error(struct mce_event *e)
{
	uint32_t err_number, siod_id, ccd_id, core_id;
	char *cache_str = "";

	if (e->bank == MCE_PL2_CACHE_BANK) {
		cache_str = "PL2";
		err_number = EXTRACT(e->status, 24, 25);

		mce_snprintf(e->mcastatus_msg, "occurred %s %s",
			     cache_str,
			     err_number < ARRAY_SIZE(cache_error_str) ?
			     cache_error_str[err_number] : "Unknown error");
	} else if (e->bank == MCE_LLC_CACHE_BANK) {
		cache_str = "LLC";
		err_number = EXTRACT(e->status, 25, 26);
		siod_id = EXTRACT(e->status, 23, 24);
		ccd_id = EXTRACT(e->status, 19, 22);
		core_id = EXTRACT(e->status, 16, 18);

		mce_snprintf(e->mcastatus_msg, "siod_id %d ccd_id %d core_id %d occurred %s %s",
			     siod_id, ccd_id, core_id, cache_str,
			     err_number < ARRAY_SIZE(cache_error_str) ?
			     cache_error_str[err_number] : "Unknown error");
	}
}

static void decode_pcie_error(struct mce_event *e)
{
	uint32_t error_code = EXTRACT(e->status, 16, 23);
	uint32_t segment, bus, device, func;

	if (!(e->status & MCI_STATUS_MISCV))
		return;

	func = EXTRACT(e->misc, 16, 18);
	device = EXTRACT(e->misc, 19, 23);
	bus = EXTRACT(e->misc, 24, 31);
	segment = EXTRACT(e->misc, 32, 39);
	mce_snprintf(e->mcastatus_msg, "Device %x:%x:%x.%x occurred pcie %s",
		     segment, bus, device, func,
		     error_code < ARRAY_SIZE(pcie_error_str) ?
		     pcie_error_str[error_code] : "Unknown Error");
}

static void decode_zdi_zpi_error(struct mce_event *e)
{
	char *zdi_zpi_str = "";
	uint32_t error_code = EXTRACT(e->status, 16, 31);
	uint32_t siod_id, ccd_id, core_id;
	uint32_t zdi_zpi_number_code;
	int idx = 0;

	switch (e->bank) {
	case MCE_CRMCA0_IOD_ZDI_BANK:
	case MCE_CRMCA1_IOD_ZDI_BANK:
		zdi_zpi_str = "iod_zdi";
		mce_snprintf(e->mcastatus_msg, "%s", zdi_zpi_str);

		if (e->status & MCI_STATUS_MISCV) {
			zdi_zpi_number_code = EXTRACT(e->misc, 25, 30);
			for (idx = 0; idx < IOD_ZDI_DEV_COUNT; idx++) {
				if (test_prefix(idx, zdi_zpi_number_code))
					break;
			}
			if (idx < IOD_ZDI_DEV_COUNT)
				mce_snprintf(e->mcastatus_msg, "zdi%d", idx);
		}

		mce_snprintf(e->mcastatus_msg, "occurred %s",
			     error_code < ARRAY_SIZE(iod_zdi_zpi_error_str) ?
			     iod_zdi_zpi_error_str[error_code] : "Unknown error");
		break;
	case MCE_CCD_ZDI_BANK:
		zdi_zpi_str = "ccd_zdi";
		core_id = EXTRACT(e->status, 16, 18);
		ccd_id = EXTRACT(e->status, 19, 22);
		siod_id = EXTRACT(e->status, 23, 24);
		error_code = EXTRACT(e->status, 25, 29);
		mce_snprintf(e->mcastatus_msg, "siod_id %d ccd_id %d core_id %d occurred %s %s",
			     siod_id, ccd_id, core_id, zdi_zpi_str,
			     error_code < ARRAY_SIZE(ccd_zdi_error_str) ?
			     ccd_zdi_error_str[error_code] : "Unknown error");
		break;
	case MCE_ZPI_BANK:
		zdi_zpi_str = "zpi";
		mce_snprintf(e->mcastatus_msg, "%s", zdi_zpi_str);

		if (e->status & MCI_STATUS_MISCV) {
			zdi_zpi_number_code = EXTRACT(e->misc, 25, 27);
			for (idx = 0; idx < ZPI_DEV_COUNT; idx++) {
				if (test_prefix(idx, zdi_zpi_number_code))
					break;
			}
			if (idx < ZPI_DEV_COUNT)
				mce_snprintf(e->mcastatus_msg, "zpi%d", idx);
		}

		mce_snprintf(e->mcastatus_msg, "occurred %s",
			     error_code < ARRAY_SIZE(iod_zdi_zpi_error_str) ?
			     iod_zdi_zpi_error_str[error_code] : "Unknown error");
		break;
	default:
		break;
	}
}

static void decode_svid_error(struct mce_event *e)
{
	uint32_t error_code = EXTRACT(e->status, 24, 31);
	char *svid_str = "";

	switch (e->bank) {
	case MCE_SVID0_BANK:
		svid_str = "svid 0";
		break;
	case MCE_SVID1_BANK:
		svid_str = "svid 1";
		break;
	case MCE_SVID2_BANK:
		svid_str = "svid 2";
	default:
		break;
	}

	mce_snprintf(e->mcastatus_msg, "%s", svid_str);

	if (e->status & MCI_STATUS_MISCV)
		mce_snprintf(e->mcastatus_msg, "vrm %d", (uint32_t)EXTRACT(e->misc, 24, 25));

	mce_snprintf(e->mcastatus_msg, "occurred %s",
		     error_code < ARRAY_SIZE(svid_error_str) ?
		     svid_error_str[error_code] : "Unknown error");
}

static void decode_mem_error(struct mce_event *e)
{
	uint32_t mca_err_code = EXTRACT(e->status, 0, 15);
	uint32_t channel_code, mem_err_code;
	uint32_t inter_err_code = EXTRACT(e->status, 16, 31);

	/* decode mca error code */
	if (mca_err_code == 0x1) {
		mce_snprintf(e->mcastatus_msg, "Memory Error, MCA Error Code: DVAD Error");
	} else if (mca_err_code == 0x5) {
		mce_snprintf(e->mcastatus_msg, "Memory Error, MCA Error Code: Parity Error");
	} else if (mca_err_code & (1 << 7)) {
		channel_code = EXTRACT(mca_err_code, 0, 3);
		mem_err_code = EXTRACT(mca_err_code, 4, 6);
		mce_snprintf(e->mcastatus_msg, "Memory Error, MCA Error Code: %s %s",
			     channel_code < ARRAY_SIZE(mem_channel_str) ?
			     mem_channel_str[channel_code] : "Unknown channel",
			     mem_err_code < ARRAY_SIZE(mem_error_mccod_str) ?
			     mem_error_mccod_str[mem_err_code] : "Unknown error");
	}

	/* decode internal error code */
	mce_snprintf(e->mcastatus_msg, ", Internal Error Code: %s",
		     inter_err_code < ARRAY_SIZE(mem_specific_error_str) ?
		     mem_specific_error_str[inter_err_code] : "Unknown error");

	if (e->status & MCI_STATUS_MISCV) {
		mce_snprintf(e->mcastatus_msg, ", CRC/PAR MRP Log: %llu", EXTRACT(e->misc, 31, 63));
		mce_snprintf(e->mcastatus_msg, ", Rank: %llu", EXTRACT(e->misc, 25, 26));
		mce_snprintf(e->mcastatus_msg, ", ECC ERROR Syndrome Single Bit: %llu", EXTRACT(e->misc, 9, 16));
		mce_snprintf(e->mcastatus_msg, ", Addr Mode: %s", EXTRACT(e->misc, 6, 8) == 2 ? "SVA" : "Unknown");
	}
}

static void decode_hif_error(struct mce_event *e)
{
	uint32_t error_code = EXTRACT(e->status, 16, 23);
	char *hif_str = "";

	switch (e->bank) {
	case MCE_HIF_H0_BANK:
		hif_str = "HIF 0";
		break;
	case MCE_HIF_H1_BANK:
		hif_str = "HIF 1";
		break;
	case MCE_HIF_H2_BANK:
		hif_str = "HIF 2";
		break;
	case MCE_HIF_H3_BANK:
		hif_str = "HIF 3";
		break;
	default:
		break;
	}

	mce_snprintf(e->mcastatus_msg, "%s occurred %s", hif_str,
		     error_code < ARRAY_SIZE(hif_error_str) ?
		     hif_error_str[error_code] : "Unknown error");
}

void kh50000_decode_model(struct mce_event *e)
{
	bank_name(e);

	switch (e->bank) {
	case MCE_CPU_BANK:
		decode_cpu_error(e);
		break;
	case MCE_PL2_CACHE_BANK:
	case MCE_LLC_CACHE_BANK:
		decode_cache_error(e);
		break;
	case MCE_CRMCA0_PCIE_BANK:
	case MCE_CRMCA1_PCIE_BANK:
		decode_pcie_error(e);
		break;
	case MCE_CRMCA0_IOD_ZDI_BANK:
	case MCE_CRMCA1_IOD_ZDI_BANK:
	case MCE_CCD_ZDI_BANK:
	case MCE_ZPI_BANK:
		decode_zdi_zpi_error(e);
		break;
	case MCE_SVID0_BANK:
	case MCE_SVID1_BANK:
	case MCE_SVID2_BANK:
		decode_svid_error(e);
		break;
	case MCE_DRAM_BANK0:
	case MCE_DRAM_BANK1:
	case MCE_DRAM_BANK2:
	case MCE_DRAM_BANK3:
	case MCE_DRAM_BANK4:
	case MCE_DRAM_BANK5:
	case MCE_DRAM_BANK6:
	case MCE_DRAM_BANK7:
	case MCE_DRAM_BANK8:
	case MCE_DRAM_BANK9:
	case MCE_DRAM_BANK10:
	case MCE_DRAM_BANK11:
		decode_mem_error(e);
		break;
	case MCE_HIF_H0_BANK:
	case MCE_HIF_H1_BANK:
	case MCE_HIF_H2_BANK:
	case MCE_HIF_H3_BANK:
		decode_hif_error(e);
		break;
	default:
		break;
	}
}
