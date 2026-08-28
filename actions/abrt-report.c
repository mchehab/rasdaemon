// SPDX-License-Identifier: GPL-2.0-only

/*
 * Copyright (c) 2016, The Linux Foundation. All rights reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/utsname.h>
#include <unistd.h>

#include "actions/abrt-report.h"
#include "core/modules.h"
#include "core/ras-logger.h"
#include "events-arch-arm/ras-arm-handler.h"
#include "events-arch-arm/ras-non-standard-handler.h"
#include "events-arch-riscv/ras-reri-handler.h"
#include "events-arch-x86/ras-mce-handler.h"
#include "events/ras-aer-handler.h"
#include "events/ras-cxl-handler.h"
#include "events/ras-devlink-handler.h"
#include "events/ras-diskerror-handler.h"
#include "events/ras-extlog-handler.h"
#include "events/ras-memory-failure-handler.h"
#include "events/ras-mc-handler.h"
#include "events/ras-signal-handler.h"

/* ABRT's local problem-reporting socket and protocol buffer limits. */
#define ABRT_SOCKET "/var/run/abrt/abrt.socket"
#define MAX_BACKTRACE_SIZE (1024 * 1024)
#define MAX_MESSAGE_SIZE (4 * MAX_BACKTRACE_SIZE)
#define INPUT_BUFFER_SIZE (8 * 1024)

/* Open one connection per report; ABRT treats each connection as a problem. */
static int setup_report_socket(void)
{
	int sockfd = -1;
	int rc = -1;
	struct sockaddr_un addr;

	sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (sockfd < 0)
		return -1;

	memset(&addr, 0, sizeof(struct sockaddr_un));
	addr.sun_family = AF_UNIX;
	strscpy(addr.sun_path, ABRT_SOCKET, sizeof(addr.sun_path));
	addr.sun_path[sizeof(addr.sun_path) - 1] = '\0';

	rc = connect(sockfd, (struct sockaddr *)&addr, sizeof(struct sockaddr_un));
	if (rc < 0) {
		close(sockfd);
		return -1;
	}

	return sockfd;
}

static int commit_report_basic(int sockfd)
{
	char *buf;
	struct utsname un;
	int rc = -1;

	if (sockfd < 0)
		return rc;

	buf = calloc(1, INPUT_BUFFER_SIZE);
	if (!buf) {
		log(TERM, LOG_ERR, "Failed to allocate memory for basic report\n");
		return -1;
	}

	memset(&un, 0, sizeof(struct utsname));

	rc = uname(&un);
	if (rc < 0) {
		free(buf);
		return rc;
	}

	/* Start ABRT's NUL-delimited PUT request and attach common fields. */
	snprintf(buf, INPUT_BUFFER_SIZE, "PUT / HTTP/1.1\r\n\r\n");
	rc = write(sockfd, buf, strlen(buf));
	if (rc < strlen(buf)) {
		free(buf);
		return -1;
	}

	snprintf(buf, INPUT_BUFFER_SIZE, "PID=%d", (int)getpid());
	rc = write(sockfd, buf, strlen(buf) + 1);
	if (rc < strlen(buf) + 1) {
		free(buf);
		return -1;
	}

	snprintf(buf, INPUT_BUFFER_SIZE, "EXECUTABLE=/boot/vmlinuz-%s",
		 un.release);
	rc = write(sockfd, buf, strlen(buf) + 1);
	if (rc < strlen(buf) + 1) {
		free(buf);
		return -1;
	}

	snprintf(buf, INPUT_BUFFER_SIZE, "TYPE=%s", "ras");
	rc = write(sockfd, buf, strlen(buf) + 1);
	if (rc < strlen(buf) + 1) {
		free(buf);
		return -1;
	}

	free(buf);
	return 0;
}

static int set_mc_event_backtrace(char *buf, struct ras_mc_event *ev)
{
	unsigned int size = MAX_BACKTRACE_SIZE;

	if (!buf || !ev)
		return -1;

	while (*buf && size > 0) {
		buf++;
		size--;
	}

	snprintf(buf, size, "BACKTRACE="
		"timestamp=%s\n"
		"error_count=%d\n"
		"error_type=%s\n"
		"msg=%s\n"
		"label=%s\n"
		"mc_index=%u\n"
		"top_layer=%d\n"
		"middle_layer=%d\n"
		"lower_layer=%d\n"
		"address=%llu\n"
		"grain=%llu\n"
		"syndrome=%llu\n"
		"driver_detail=%s\n",
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

	return 0;
}

static int set_mce_event_backtrace(char *buf, struct mce_event *ev)
{
	unsigned int size = MAX_BACKTRACE_SIZE;

	if (!buf || !ev)
		return -1;

	while (*buf && size > 0) {
		buf++;
		size--;
	}

	snprintf(buf, size, "BACKTRACE="
		"timestamp=%s\n"
		"bank_name=%s\n"
		"error_msg=%s\n"
		"mcgstatus_msg=%s\n"
		"mcistatus_msg=%s\n"
		"mcastatus_msg=%s\n"
		"user_action=%s\n"
		"mc_location=%s\n"
		"mcgcap=%lu\n"
		"mcgstatus=%lu\n"
		"status=%lu\n"
		"addr=%lu\n"
		"misc=%lu\n"
		"ip=%lu\n"
		"tsc=%lu\n"
		"walltime=%lu\n"
		"cpu=%u\n"
		"cpuid=%u\n"
		"apicid=%u\n"
		"socketid=%u\n"
		"cs=%d\n"
		"bank=%d\n"
		"cpuvendor=%d\n",
		ev->timestamp,
		ev->bank_name,
		ev->error_msg,
		ev->mcgstatus_msg,
		ev->mcistatus_msg,
		ev->mcastatus_msg,
		ev->user_action,
		ev->mc_location,
		ev->mcgcap,
		ev->mcgstatus,
		ev->status,
		ev->addr,
		ev->misc,
		ev->ip,
		ev->tsc,
		ev->walltime,
		ev->cpu,
		ev->cpuid,
		ev->apicid,
		ev->socketid,
		ev->cs,
		ev->bank,
		ev->cpuvendor);

	return 0;
}

static int set_aer_event_backtrace(char *buf, struct ras_aer_event *ev)
{
	unsigned int size = MAX_BACKTRACE_SIZE;

	if (!buf || !ev)
		return -1;

	while (*buf && size > 0) {
		buf++;
		size--;
	}

	snprintf(buf, size, "BACKTRACE="
		"timestamp=%s\n"
		"error_type=%s\n"
		"dev_name=%s\n"
		"msg=%s\n",
		ev->timestamp,
		ev->error_type,
		ev->dev_name,
		ev->msg);

	return 0;
}

static int set_non_standard_event_backtrace(char *buf, struct ras_non_standard_event *ev)
{
	unsigned int size = MAX_BACKTRACE_SIZE;

	if (!buf || !ev)
		return -1;

	while (*buf && size > 0) {
		buf++;
		size--;
	}

	snprintf(buf, size, "BACKTRACE="
		"timestamp=%s\n"
		"severity=%s\n"
		"length=%d\n",
		ev->timestamp,
		ev->severity,
		ev->length);

	return 0;
}

static int set_arm_event_backtrace(char *buf, struct ras_arm_event *ev)
{
	unsigned int size = MAX_BACKTRACE_SIZE;

	if (!buf || !ev)
		return -1;

	while (*buf && size > 0) {
		buf++;
		size--;
	}

	snprintf(buf, size, "BACKTRACE="
		"timestamp=%s\n"
		"error_count=%d\n"
		"affinity=%d\n"
		"mpidr=0x%lx\n"
		"midr=0x%lx\n"
		"running_state=%d\n"
		"psci_state=%d\n",
		ev->timestamp,
		ev->error_count,
		ev->affinity,
		ev->mpidr,
		ev->midr,
		ev->running_state,
		ev->psci_state);

	return 0;
}

static int set_devlink_event_backtrace(char *buf, struct devlink_event *ev)
{
	unsigned int size = MAX_BACKTRACE_SIZE;

	if (!buf || !ev)
		return -1;

	while (*buf && size > 0) {
		buf++;
		size--;
	}

	snprintf(buf, size, "BACKTRACE="
		"timestamp=%s\n"
		"bus_name=%s\n"
		"dev_name=%s\n"
		"driver_name=%s\n"
		"reporter_name=%s\n"
		"msg=%s\n",
		ev->timestamp,
		ev->bus_name,
		ev->dev_name,
		ev->driver_name,
		ev->reporter_name,
		ev->msg);

	return 0;
}

static int set_diskerror_event_backtrace(char *buf, struct diskerror_event *ev)
{
	unsigned int size = MAX_BACKTRACE_SIZE;

	if (!buf || !ev)
		return -1;

	while (*buf && size > 0) {
		buf++;
		size--;
	}

	snprintf(buf, size, "BACKTRACE="
		"timestamp=%s\n"
		"dev=%s\n"
		"sector=%llu\n"
		"nr_sector=%u\n"
		"error=%s\n"
		"rwbs=%s\n"
		"cmd=%s\n",
		ev->timestamp,
		ev->dev,
		ev->sector,
		ev->nr_sector,
		ev->error,
		ev->rwbs,
		ev->cmd);

	return 0;
}

static int set_mf_event_backtrace(char *buf, struct ras_mf_event *ev)
{
	unsigned int size = MAX_BACKTRACE_SIZE;

	if (!buf || !ev)
		return -1;

	while (*buf && size > 0) {
		buf++;
		size--;
	}

	snprintf(buf, size, "BACKTRACE="
		"timestamp=%s\n"
		"pfn=%s\n"
		"page_type=%s\n"
		"action_result=%s\n",
		ev->timestamp,
		ev->pfn,
		ev->page_type,
		ev->action_result);

	return 0;
}

static int set_cxl_poison_event_backtrace(char *buf, struct ras_cxl_poison_event *ev)
{
	unsigned int size = MAX_BACKTRACE_SIZE;

	if (!buf || !ev)
		return -1;

	while (*buf && size > 0) {
		buf++;
		size--;
	}

	snprintf(buf, size, "BACKTRACE="
		"timestamp=%s\n"
		"memdev=%s\n"
		"host=%s\n"
		"serial=0x%lx\n"
		"trace_type=%s\n"
		"region=%s\n"
		"region_uuid=%s\n"
		"hpa=0x%lx\n"
		"hpa_alias0=0x%lx\n"
		"dpa=0x%lx\n"
		"dpa_length=0x%x\n"
		"source=%s\n"
		"flags=%u\n"
		"overflow_timestamp=%s\n",
		ev->timestamp,
		ev->memdev,
		ev->host,
		ev->serial,
		ev->trace_type,
		ev->region,
		ev->uuid,
		ev->hpa,
		ev->hpa_alias0,
		ev->dpa,
		ev->dpa_length,
		ev->source,
		ev->flags,
		ev->overflow_ts);

	return 0;
}

static int set_cxl_aer_ue_event_backtrace(char *buf, struct ras_cxl_aer_ue_event *ev)
{
	unsigned int size = MAX_BACKTRACE_SIZE;

	if (!buf || !ev)
		return -1;

	while (*buf && size > 0) {
		buf++;
		size--;
	}

	snprintf(buf, size, "BACKTRACE="
		"timestamp=%s\n"
		"memdev=%s\n"
		"host=%s\n"
		"serial=0x%lx\n"
		"error_status=%u\n"
		"first_error=%u\n",
		ev->timestamp,
		ev->memdev,
		ev->host,
		ev->serial,
		ev->error_status,
		ev->first_error);

	return 0;
}

static int set_cxl_aer_ce_event_backtrace(char *buf, struct ras_cxl_aer_ce_event *ev)
{
	unsigned int size = MAX_BACKTRACE_SIZE;

	if (!buf || !ev)
		return -1;

	while (*buf && size > 0) {
		buf++;
		size--;
	}

	snprintf(buf, size, "BACKTRACE="
		"timestamp=%s\n"
		"memdev=%s\n"
		"host=%s\n"
		"serial=0x%lx\n"
		"error_status=%u\n",
		ev->timestamp,
		ev->memdev,
		ev->host,
		ev->serial,
		ev->error_status);

	return 0;
}

static int set_cxl_overflow_event_backtrace(char *buf, struct ras_cxl_overflow_event *ev)
{
	unsigned int size = MAX_BACKTRACE_SIZE;

	if (!buf || !ev)
		return -1;

	while (*buf && size > 0) {
		buf++;
		size--;
	}

	snprintf(buf, size, "BACKTRACE="
		"timestamp=%s\n"
		"memdev=%s\n"
		"host=%s\n"
		"serial=0x%lx\n"
		"log_type=%s\n"
		"count=%u\n"
		"first_ts=%s\n"
		"last_ts=%s\n",
		ev->timestamp,
		ev->memdev,
		ev->host,
		ev->serial,
		ev->log_type,
		ev->count,
		ev->first_ts,
		ev->last_ts);

	return 0;
}

static int set_cxl_generic_event_backtrace(char *buf, struct ras_cxl_generic_event *ev)
{
	unsigned int size = MAX_BACKTRACE_SIZE;

	if (!buf || !ev)
		return -1;

	while (*buf && size > 0) {
		buf++;
		size--;
	}

	snprintf(buf, size, "BACKTRACE="
		"timestamp=%s\n"
		"memdev=%s\n"
		"host=%s\n"
		"serial=0x%lx\n"
		"log_type=%s\n"
		"hdr_uuid=%s\n"
		"hdr_flags=0x%x\n"
		"hdr_handle=0x%x\n"
		"hdr_related_handle=0x%x\n"
		"hdr_timestamp=%s\n"
		"hdr_length=%u\n"
		"hdr_maint_op_class=%u\n"
		"hdr_maint_op_sub_class=%u\n"
		"hdr_ld_id=0x%x\n"
		"hdr_head_id=0x%x\n",
		ev->hdr.timestamp,
		ev->hdr.memdev,
		ev->hdr.host,
		ev->hdr.serial,
		ev->hdr.log_type,
		ev->hdr.hdr_uuid,
		ev->hdr.hdr_flags,
		ev->hdr.hdr_handle,
		ev->hdr.hdr_related_handle,
		ev->hdr.hdr_timestamp,
		ev->hdr.hdr_length,
		ev->hdr.hdr_maint_op_class,
		ev->hdr.hdr_maint_op_sub_class,
		ev->hdr.hdr_ld_id,
		ev->hdr.hdr_head_id);

	return 0;
}

static int set_cxl_general_media_event_backtrace(char *buf, struct ras_cxl_general_media_event *ev)
{
	unsigned int size = MAX_BACKTRACE_SIZE;

	if (!buf || !ev)
		return -1;

	while (*buf && size > 0) {
		buf++;
		size--;
	}

	snprintf(buf, size, "BACKTRACE="
		"timestamp=%s\n"
		"memdev=%s\n"
		"host=%s\n"
		"serial=0x%lx\n"
		"log_type=%s\n"
		"hdr_uuid=%s\n"
		"hdr_flags=0x%x\n"
		"hdr_handle=0x%x\n"
		"hdr_related_handle=0x%x\n"
		"hdr_timestamp=%s\n"
		"hdr_length=%u\n"
		"hdr_maint_op_class=%u\n"
		"hdr_maint_op_sub_class=%u\n"
		"hdr_ld_id=0x%x\n"
		"hdr_head_id=0x%x\n"
		"dpa=0x%lx\n"
		"dpa_flags=%u\n"
		"descriptor=%u\n"
		"type=%u\n"
		"sub_type=0x%x\n"
		"transaction_type=%u\n"
		"hpa=0x%lx\n"
		"hpa_alias0=0x%lx\n"
		"region=%s\n"
		"region_uuid=%s\n"
		"channel=%u\n"
		"rank=%u\n"
		"device=0x%x\n"
		"cme_threshold_ev_flags=0x%x\n"
		"cme_count=0x%x\n",
		ev->hdr.timestamp,
		ev->hdr.memdev,
		ev->hdr.host,
		ev->hdr.serial,
		ev->hdr.log_type,
		ev->hdr.hdr_uuid,
		ev->hdr.hdr_flags,
		ev->hdr.hdr_handle,
		ev->hdr.hdr_related_handle,
		ev->hdr.hdr_timestamp,
		ev->hdr.hdr_length,
		ev->hdr.hdr_maint_op_class,
		ev->hdr.hdr_maint_op_sub_class,
		ev->hdr.hdr_ld_id,
		ev->hdr.hdr_head_id,
		ev->dpa,
		ev->dpa_flags,
		ev->descriptor,
		ev->type,
		ev->sub_type,
		ev->transaction_type,
		ev->hpa,
		ev->hpa_alias0,
		ev->region,
		ev->region_uuid,
		ev->channel,
		ev->rank,
		ev->device,
		ev->cme_threshold_ev_flags,
		ev->cme_count);

	return 0;
}

static int set_cxl_dram_event_backtrace(char *buf, struct ras_cxl_dram_event *ev)
{
	unsigned int size = MAX_BACKTRACE_SIZE;

	if (!buf || !ev)
		return -1;

	while (*buf && size > 0) {
		buf++;
		size--;
	}

	snprintf(buf, size, "BACKTRACE="
		"timestamp=%s\n"
		"memdev=%s\n"
		"host=%s\n"
		"serial=0x%lx\n"
		"log_type=%s\n"
		"hdr_uuid=%s\n"
		"hdr_flags=0x%x\n"
		"hdr_handle=0x%x\n"
		"hdr_related_handle=0x%x\n"
		"hdr_timestamp=%s\n"
		"hdr_length=%u\n"
		"hdr_maint_op_class=%u\n"
		"hdr_maint_op_sub_class=%u\n"
		"hdr_ld_id=0x%x\n"
		"hdr_head_id=0x%x\n"
		"dpa=0x%lx\n"
		"dpa_flags=%u\n"
		"descriptor=%u\n"
		"type=%u\n"
		"sub_type=0x%x\n"
		"transaction_type=%u\n"
		"hpa=0x%lx\n"
		"hpa_alias0=0x%lx\n"
		"region=%s\n"
		"region_uuid=%s\n"
		"channel=%u\n"
		"sub_channel=%u\n"
		"rank=%u\n"
		"nibble_mask=%u\n"
		"bank_group=%u\n"
		"bank=%u\n"
		"row=%u\n"
		"column=%u\n"
		"cme_threshold_ev_flags=0x%x\n"
		"cvme_count=0x%x\n",
		ev->hdr.timestamp,
		ev->hdr.memdev,
		ev->hdr.host,
		ev->hdr.serial,
		ev->hdr.log_type,
		ev->hdr.hdr_uuid,
		ev->hdr.hdr_flags,
		ev->hdr.hdr_handle,
		ev->hdr.hdr_related_handle,
		ev->hdr.hdr_timestamp,
		ev->hdr.hdr_length,
		ev->hdr.hdr_maint_op_class,
		ev->hdr.hdr_maint_op_sub_class,
		ev->hdr.hdr_ld_id,
		ev->hdr.hdr_head_id,
		ev->dpa,
		ev->dpa_flags,
		ev->descriptor,
		ev->type,
		ev->sub_type,
		ev->transaction_type,
		ev->hpa,
		ev->hpa_alias0,
		ev->region,
		ev->region_uuid,
		ev->channel,
		ev->sub_channel,
		ev->rank,
		ev->nibble_mask,
		ev->bank_group,
		ev->bank,
		ev->row,
		ev->column,
		ev->cme_threshold_ev_flags,
		ev->cvme_count);

	return 0;
}

static int set_cxl_memory_module_event_backtrace(char *buf, struct ras_cxl_memory_module_event *ev)
{
	unsigned int size = MAX_BACKTRACE_SIZE;

	if (!buf || !ev)
		return -1;

	while (*buf && size > 0) {
		buf++;
		size--;
	}

	snprintf(buf, size, "BACKTRACE="
		"timestamp=%s\n"
		"memdev=%s\n"
		"host=%s\n"
		"serial=0x%lx\n"
		"log_type=%s\n"
		"hdr_uuid=%s\n"
		"hdr_flags=0x%x\n"
		"hdr_handle=0x%x\n"
		"hdr_related_handle=0x%x\n"
		"hdr_timestamp=%s\n"
		"hdr_length=%u\n"
		"hdr_maint_op_class=%u\n"
		"hdr_maint_op_sub_class=%u\n"
		"hdr_ld_id=0x%x\n"
		"hdr_head_id=0x%x\n"
		"event_type=%u\n"
		"event_sub_type=0x%x\n"
		"health_status=%u\n"
		"media_status=%u\n"
		"life_used=%u\n"
		"dirty_shutdown_cnt=%u\n"
		"cor_vol_err_cnt=%u\n"
		"cor_per_err_cnt=%u\n"
		"device_temp=%d\n"
		"add_status=%u\n",
		ev->hdr.timestamp,
		ev->hdr.memdev,
		ev->hdr.host,
		ev->hdr.serial,
		ev->hdr.log_type,
		ev->hdr.hdr_uuid,
		ev->hdr.hdr_flags,
		ev->hdr.hdr_handle,
		ev->hdr.hdr_related_handle,
		ev->hdr.hdr_timestamp,
		ev->hdr.hdr_length,
		ev->hdr.hdr_maint_op_class,
		ev->hdr.hdr_maint_op_sub_class,
		ev->hdr.hdr_ld_id,
		ev->hdr.hdr_head_id,
		ev->event_type,
		ev->event_sub_type,
		ev->health_status,
		ev->media_status,
		ev->life_used,
		ev->dirty_shutdown_cnt,
		ev->cor_vol_err_cnt,
		ev->cor_per_err_cnt,
		ev->device_temp,
		ev->add_status);

	return 0;
}

static int set_cxl_sparing(char *buf, struct ras_cxl_memory_sparing_event *ev)
{
	unsigned int size = MAX_BACKTRACE_SIZE;

	if (!buf || !ev)
		return -1;

	while (*buf && size > 0) {
		buf++;
		size--;
	}

	snprintf(buf, size, "BACKTRACE="
		"timestamp=%s\n"
		"memdev=%s\n"
		"host=%s\n"
		"serial=0x%lx\n"
		"log_type=%s\n"
		"hdr_uuid=%s\n"
		"hdr_flags=0x%x\n"
		"hdr_handle=0x%x\n"
		"hdr_related_handle=0x%x\n"
		"hdr_timestamp=%s\n"
		"hdr_length=%u\n"
		"hdr_maint_op_class=%u\n"
		"hdr_maint_op_sub_class=%u\n"
		"hdr_ld_id=0x%x\n"
		"hdr_head_id=0x%x\n"
		"flags=0x%x\n"
		"result=0x%x\n"
		"validity_flags=0x%x\n"
		"resources_available=%u\n"
		"channel=%u\n"
		"sub_channel=%u\n"
		"rank=%u\n"
		"nibble_mask=0x%x\n"
		"bank_group=%u\n"
		"bank=%u\n"
		"row=%u\n"
		"column=%u\n",
		ev->hdr.timestamp,
		ev->hdr.memdev,
		ev->hdr.host,
		ev->hdr.serial,
		ev->hdr.log_type,
		ev->hdr.hdr_uuid,
		ev->hdr.hdr_flags,
		ev->hdr.hdr_handle,
		ev->hdr.hdr_related_handle,
		ev->hdr.hdr_timestamp,
		ev->hdr.hdr_length,
		ev->hdr.hdr_maint_op_class,
		ev->hdr.hdr_maint_op_sub_class,
		ev->hdr.hdr_ld_id,
		ev->hdr.hdr_head_id,
		ev->flags,
		ev->result,
		ev->validity_flags,
		ev->res_avail,
		ev->channel,
		ev->sub_channel,
		ev->rank,
		ev->nibble_mask,
		ev->bank_group,
		ev->bank,
		ev->row,
		ev->column);

	return 0;
}

static int set_extlog_event_backtrace(char *buf, struct ras_extlog_event *ev)
{
	unsigned int size = MAX_BACKTRACE_SIZE;

	if (!buf || !ev)
		return -1;

	while (*buf && size > 0) {
		buf++;
		size--;
	}

	snprintf(buf, size, "BACKTRACE="
		"timestamp=%s\n"
		"error_sequence=%d\n"
		"error_type=%d\n"
		"severity=%d\n"
		"address=0x%llx\n"
		"physical_address_mask_lsb=%d\n"
		"fru_text=%s\n"
		"cper_data_length=%u\n",
		ev->timestamp,
		ev->error_seq,
		ev->etype,
		ev->severity,
		ev->address,
		ev->pa_mask_lsb,
		ev->fru_text,
		ev->cper_data_length);

	return 0;
}

static int set_signal_event_backtrace(char *buf, struct ras_signal_event *ev)
{
	unsigned int size = MAX_BACKTRACE_SIZE;

	if (!buf || !ev)
		return -1;

	while (*buf && size > 0) {
		buf++;
		size--;
	}

	snprintf(buf, size, "BACKTRACE="
		"timestamp=%s\n"
		"signal=%d\n"
		"errorno=%d\n"
		"code=%d\n"
		"comm=%s\n"
		"grp=%d\n"
		"res=%d\n",
		ev->timestamp,
		ev->sig,
		ev->error_no,
		ev->code,
		ev->comm,
		ev->group,
		ev->result);

	return 0;
}

static int format_report_backtrace(char *buf, int type, void *ev)
{
	switch (type) {
	case MC_EVENT:
		return set_mc_event_backtrace(buf, ev);
	case AER_EVENT:
		return set_aer_event_backtrace(buf, ev);
	case MCE_EVENT:
		return set_mce_event_backtrace(buf, ev);
	case NON_STANDARD_EVENT:
		return set_non_standard_event_backtrace(buf, ev);
	case ARM_EVENT:
		return set_arm_event_backtrace(buf, ev);
	case EXTLOG_EVENT:
		return set_extlog_event_backtrace(buf, ev);
	case DEVLINK_EVENT:
		return set_devlink_event_backtrace(buf, ev);
	case DISKERROR_EVENT:
		return set_diskerror_event_backtrace(buf, ev);
	case MF_EVENT:
		return set_mf_event_backtrace(buf, ev);
	case CXL_POISON_EVENT:
		return set_cxl_poison_event_backtrace(buf, ev);
	case CXL_AER_UE_EVENT:
		return set_cxl_aer_ue_event_backtrace(buf, ev);
	case CXL_AER_CE_EVENT:
		return set_cxl_aer_ce_event_backtrace(buf, ev);
	case CXL_OVERFLOW_EVENT:
		return set_cxl_overflow_event_backtrace(buf, ev);
	case CXL_GENERIC_EVENT:
		return set_cxl_generic_event_backtrace(buf, ev);
	case CXL_GENERAL_MEDIA_EVENT:
		return set_cxl_general_media_event_backtrace(buf, ev);
	case CXL_DRAM_EVENT:
		return set_cxl_dram_event_backtrace(buf, ev);
	case CXL_MEMORY_MODULE_EVENT:
		return set_cxl_memory_module_event_backtrace(buf, ev);
	case CXL_MEMORY_SPARING_EVENT:
		return set_cxl_sparing(buf, ev);
	case SIGNAL_EVENT:
		return set_signal_event_backtrace(buf, ev);
	default:
		return -1;
	}
}

#ifdef HAVE_UNITTEST
int abrt_report_test_format(int type, void *event, char *output, size_t size)
{
	char *buf;
	int rc;

	if (!event || !output || !size)
		return -1;
	buf = calloc(1, MAX_BACKTRACE_SIZE);
	if (!buf)
		return -1;
	rc = format_report_backtrace(buf, type, event);
	if (!rc)
		strscpy(output, buf, size);
	free(buf);
	return rc;
}
#endif

static int commit_report_backtrace(int sockfd, int type, void *ev)
{
	char *buf;
	char *pbuf;
	int rc = -1;
	int buf_len = 0;

	if (sockfd < 0 || !ev)
		return -1;

	buf = calloc(1, MAX_BACKTRACE_SIZE);
	if (!buf) {
		log(TERM, LOG_ERR, "Failed to allocate memory for backtrace report\n");
		return -1;
	}
	pbuf = buf;

	rc = format_report_backtrace(buf, type, ev);

	if (rc < 0) {
		free(buf);
		return -1;
	}

	buf_len = strlen(buf);

	for (; buf_len > INPUT_BUFFER_SIZE - 1; buf_len -= (INPUT_BUFFER_SIZE - 1)) {
		rc = write(sockfd, pbuf, INPUT_BUFFER_SIZE - 1);
		if (rc < INPUT_BUFFER_SIZE - 1) {
			free(buf);
			return -1;
		}

		pbuf = pbuf + INPUT_BUFFER_SIZE - 1;
	}

	rc = write(sockfd, pbuf, buf_len + 1);
	if (rc < buf_len) {
		free(buf);
		return -1;
	}

	free(buf);
	return 0;
}

static int commit_report_common(struct ras_events *ras, int type, void *ev, const char *analyzer, const char *reason)
{
	char *buf;
	int sockfd = -1;
	int done = 0;
	int rc = -1;

	buf = calloc(1, MAX_MESSAGE_SIZE);
	if (!buf) {
		log(TERM, LOG_ERR, "Failed to allocate memory for report\n");
		return -1;
	}

	sockfd = setup_report_socket();
	if (sockfd < 0) {
		free(buf);
		return -1;
	}

	rc = commit_report_basic(sockfd);
	if (rc < 0)
		goto fail;

	rc = commit_report_backtrace(sockfd, type, ev);
	if (rc < 0)
		goto fail;

	snprintf(buf, MAX_MESSAGE_SIZE, "ANALYZER=%s", analyzer);
	rc = write(sockfd, buf, strlen(buf) + 1);
	if (rc < strlen(buf) + 1)
		goto fail;

	snprintf(buf, MAX_MESSAGE_SIZE, "REASON=%s", reason);
	rc = write(sockfd, buf, strlen(buf) + 1);
	if (rc < strlen(buf) + 1)
		goto fail;

	done = 1;

fail:
	if (sockfd >= 0)
		close(sockfd);
	free(buf);
	return done ? 0 : -1;
}

struct report_description {
	const char *analyzer;
	const char *reason;
};

static const struct report_description report_descriptions[NR_EVENTS] = {
	[MC_EVENT] = { "rasdaemon-mc", "EDAC driver report problem" },
	[MCE_EVENT] = { "rasdaemon-mce",
			"Machine Check driver report problem" },
	[AER_EVENT] = { "rasdaemon-aer", "PCIe AER driver report problem" },
	[NON_STANDARD_EVENT] = { "rasdaemon-non-standard",
				 "Unknown CPER section problem" },
	[ARM_EVENT] = { "rasdaemon-arm", "ARM CPU report problem" },
	[EXTLOG_EVENT] = { "rasdaemon-extlog", "Extended error log event" },
	[DEVLINK_EVENT] = { "rasdaemon-devlink",
			    "devlink health report problem" },
	[DISKERROR_EVENT] = { "rasdaemon-diskerror", "disk I/O error" },
	[MF_EVENT] = { "rasdaemon-memory_failure", "memory failure problem" },
	[SIGNAL_EVENT] = { "rasdaemon-signal_event",
			   "SIGBUS for Hardware error" },
	[CXL_POISON_EVENT] = { "rasdaemon-cxl-poison", "CXL poison" },
	[CXL_AER_UE_EVENT] = { "rasdaemon-cxl-aer-uncorrectable-error",
			       "CXL AER uncorrectable error" },
	[CXL_AER_CE_EVENT] = { "rasdaemon-cxl-aer-correctable-error",
			       "CXL AER correctable error" },
	[CXL_OVERFLOW_EVENT] = { "rasdaemon-cxl-overflow", "CXL overflow" },
	[CXL_GENERIC_EVENT] = { "rasdaemon-cxl_generic_event",
				"CXL Generic Event " },
	[CXL_GENERAL_MEDIA_EVENT] = { "rasdaemon-cxl_general_media_event",
				      "CXL General Media Event" },
	[CXL_DRAM_EVENT] = { "rasdaemon-cxl_dram_event", "CXL DRAM Event" },
	[CXL_MEMORY_MODULE_EVENT] = { "rasdaemon-cxl_memory_module_event",
				       "CXL Memory Module Event" },
	[CXL_MEMORY_SPARING_EVENT] = { "rasdaemon-cxl_memory_sparing_event",
					"CXL Memory Sparing Event" },
	[RERI_EVENT] = { "rasdaemon-reri", "RISC-V RERI error report" },
};

static int abrt_report_event(struct ras_events *ras, int event, void *data)
{
	const struct report_description *description = &report_descriptions[event];

	if (event == RERI_EVENT) {
		struct ras_reri_event *reri = data;

		if (!ras->record_events || reri->severity < RERI_SEV_RECOVERABLE)
			return 0;
	}

	return commit_report_common(ras, event, data, description->analyzer,
				    description->reason);
}

#define REPORT_EVENT_MASK (BIT_ULL(MC_EVENT) | BIT_ULL(MCE_EVENT) | \
	BIT_ULL(AER_EVENT) | BIT_ULL(NON_STANDARD_EVENT) | BIT_ULL(ARM_EVENT) | \
	BIT_ULL(EXTLOG_EVENT) | \
	BIT_ULL(DEVLINK_EVENT) | BIT_ULL(DISKERROR_EVENT) | BIT_ULL(MF_EVENT) | \
	BIT_ULL(SIGNAL_EVENT) | BIT_ULL(CXL_POISON_EVENT) | \
	BIT_ULL(CXL_AER_UE_EVENT) | BIT_ULL(CXL_AER_CE_EVENT) | \
	BIT_ULL(CXL_OVERFLOW_EVENT) | BIT_ULL(CXL_GENERIC_EVENT) | \
	BIT_ULL(CXL_GENERAL_MEDIA_EVENT) | BIT_ULL(CXL_DRAM_EVENT) | \
	BIT_ULL(CXL_MEMORY_MODULE_EVENT) | \
	BIT_ULL(CXL_MEMORY_SPARING_EVENT) | BIT_ULL(RERI_EVENT))

/* Meson source selection controls whether this consumer is registered. */
static const struct ras_event_consumer abrt_report_consumer = {
	.name = "abrt-report",
	.priority = PRI_REPORTING,
	.events = REPORT_EVENT_MASK,
	.consume = abrt_report_event,
};

static int abrt_report_init(struct ras_module_ctx *ctx)
{
	return ras_event_consumer_register(&abrt_report_consumer);
}

static void abrt_report_cleanup(struct ras_module_ctx *ctx)
{
	ras_event_consumer_unregister(&abrt_report_consumer);
}

static const struct ras_module_entry abrt_report_module = {
	.name = "abrt-report",
	.level = ACTIONS_MODULE,
	.init = abrt_report_init,
	.cleanup = abrt_report_cleanup,
};

REGISTER_RAS_MODULE(abrt_report_module);
