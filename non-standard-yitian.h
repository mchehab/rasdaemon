/*
 * Copyright (C) 2023 Alibaba Inc
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */

#ifndef __NON_STANDARD_YITIAN_H
#define __NON_STANDARD_YITIAN_H

#include "ras-events.h"
#include "traceevent/event-parse.h"

#define YITIAN_RAS_TYPE_DDR		0x50

struct yitian_payload_header {
	uint8_t    type;
	uint8_t    subtype;
	uint16_t   instance;
};

struct yitian_ddr_payload_type_sec {
	struct yitian_payload_header header;
	uint32_t   ecccfg0;
	uint32_t   ecccfg1;
	uint32_t   eccstat;
	uint32_t   eccerrcnt;
	uint32_t   ecccaddr0;
	uint32_t   ecccaddr1;
	uint32_t   ecccsyn0;
	uint32_t   ecccsyn1;
	uint32_t   ecccsyn2;
	uint32_t   eccuaddr0;
	uint32_t   eccuaddr1;
	uint32_t   eccusyn0;
	uint32_t   eccusyn1;
	uint32_t   eccusyn2;
	uint32_t   eccbitmask0;
	uint32_t   eccbitmask1;
	uint32_t   eccbitmask2;
	uint32_t   adveccstat;
	uint32_t   eccapstat;
	uint32_t   ecccdata0;
	uint32_t   ecccdata1;
	uint32_t   eccudata0;
	uint32_t   eccudata1;
	uint32_t   eccsymbol;
	uint32_t   eccerrcntctl;
	uint32_t   eccerrcntstat;
	uint32_t   eccerrcnt0;
	uint32_t   eccerrcnt1;
	uint32_t   reserved0;
	uint32_t   reserved1;
	uint32_t   reserved2;
};

struct ras_yitian_ddr_payload_event {
	char timestamp[64];
	unsigned long long address;
	char *reg_msg;
};

int record_yitian_ddr_reg_dump_event(struct ras_ns_ev_decoder *ev_decoder,
				     struct ras_yitian_ddr_payload_event *ev);
void decode_yitian_ddr_payload_err_regs(struct ras_ns_ev_decoder *ev_decoder,
					struct trace_seq *s,
					const struct yitian_ddr_payload_type_sec *err,
					struct ras_events *ras);
#endif
