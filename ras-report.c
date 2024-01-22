/*
 * Copyright (c) 2016, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "ras-report.h"

static int setup_report_socket(void)
{
	int sockfd = -1;
	int rc = -1;
	struct sockaddr_un addr;

	sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (sockfd < 0) {
		return -1;
	}

	memset(&addr, 0, sizeof(struct sockaddr_un));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, ABRT_SOCKET, sizeof(addr.sun_path));
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
	char buf[INPUT_BUFFER_SIZE];
	struct utsname un;
	int rc = -1;

	if (sockfd < 0) {
		return rc;
	}

	memset(buf, 0, INPUT_BUFFER_SIZE);
	memset(&un, 0, sizeof(struct utsname));

	rc = uname(&un);
	if (rc < 0) {
		return rc;
	}

	/*
	 * ABRT server protocol
	 */
	sprintf(buf, "PUT / HTTP/1.1\r\n\r\n");
	rc = write(sockfd, buf, strlen(buf));
	if (rc < strlen(buf)) {
		return -1;
	}

	sprintf(buf, "PID=%d", (int)getpid());
	rc = write(sockfd, buf, strlen(buf) + 1);
	if (rc < strlen(buf) + 1) {
		return -1;
	}

	sprintf(buf, "EXECUTABLE=/boot/vmlinuz-%s", un.release);
	rc = write(sockfd, buf, strlen(buf) + 1);
	if (rc < strlen(buf) + 1) {
		return -1;
	}

	sprintf(buf, "TYPE=%s", "ras");
	rc = write(sockfd, buf, strlen(buf) + 1);
	if (rc < strlen(buf) + 1) {
		return -1;
	}

	return 0;
}

static int set_mc_event_backtrace(char *buf, struct ras_mc_event *ev)
{
	char bt_buf[MAX_BACKTRACE_SIZE];

	if (!buf || !ev)
		return -1;

	sprintf(bt_buf, "BACKTRACE="	\
						"timestamp=%s\n"	\
						"error_count=%d\n"	\
						"error_type=%s\n"	\
						"msg=%s\n"	\
						"label=%s\n"	\
						"mc_index=%c\n"	\
						"top_layer=%c\n"	\
						"middle_layer=%c\n"	\
						"lower_layer=%c\n"	\
						"address=%llu\n"	\
						"grain=%llu\n"	\
						"syndrome=%llu\n"	\
						"driver_detail=%s\n",	\
						ev->timestamp,	\
						ev->error_count,	\
						ev->error_type,	\
						ev->msg,	\
						ev->label,	\
						ev->mc_index,	\
						ev->top_layer,	\
						ev->middle_layer,	\
						ev->lower_layer,	\
						ev->address,	\
						ev->grain,	\
						ev->syndrome,	\
						ev->driver_detail);

	strcat(buf, bt_buf);

	return 0;
}

static int set_mce_event_backtrace(char *buf, struct mce_event *ev)
{
	char bt_buf[MAX_BACKTRACE_SIZE];

	if (!buf || !ev)
		return -1;

	sprintf(bt_buf, "BACKTRACE="	\
						"timestamp=%s\n"	\
						"bank_name=%s\n"	\
						"error_msg=%s\n"	\
						"mcgstatus_msg=%s\n"	\
						"mcistatus_msg=%s\n"	\
						"mcastatus_msg=%s\n"	\
						"user_action=%s\n"	\
						"mc_location=%s\n"	\
						"mcgcap=%lu\n"	\
						"mcgstatus=%lu\n"	\
						"status=%lu\n"	\
						"addr=%lu\n"	\
						"misc=%lu\n"	\
						"ip=%lu\n"	\
						"tsc=%lu\n"	\
						"walltime=%lu\n"	\
						"cpu=%u\n"	\
						"cpuid=%u\n"	\
						"apicid=%u\n"	\
						"socketid=%u\n"	\
						"cs=%d\n"	\
						"bank=%d\n"	\
						"cpuvendor=%d\n",	\
						ev->timestamp,	\
						ev->bank_name,	\
						ev->error_msg,	\
						ev->mcgstatus_msg,	\
						ev->mcistatus_msg,	\
						ev->mcastatus_msg,	\
						ev->user_action,	\
						ev->mc_location,	\
						ev->mcgcap,	\
						ev->mcgstatus,	\
						ev->status,	\
						ev->addr,	\
						ev->misc,	\
						ev->ip,	\
						ev->tsc,	\
						ev->walltime,	\
						ev->cpu,	\
						ev->cpuid,	\
						ev->apicid,	\
						ev->socketid,	\
						ev->cs,	\
						ev->bank,	\
						ev->cpuvendor);

	strcat(buf, bt_buf);

	return 0;
}

static int set_aer_event_backtrace(char *buf, struct ras_aer_event *ev)
{
	char bt_buf[MAX_BACKTRACE_SIZE];

	if (!buf || !ev)
		return -1;

	sprintf(bt_buf, "BACKTRACE="	\
						"timestamp=%s\n"	\
						"error_type=%s\n"	\
						"dev_name=%s\n"	\
						"msg=%s\n",	\
						ev->timestamp,	\
						ev->error_type,	\
						ev->dev_name,	\
						ev->msg);

	strcat(buf, bt_buf);

	return 0;
}

static int set_non_standard_event_backtrace(char *buf, struct ras_non_standard_event *ev)
{
	char bt_buf[MAX_BACKTRACE_SIZE];

	if (!buf || !ev)
		return -1;

	sprintf(bt_buf, "BACKTRACE="	\
						"timestamp=%s\n"	\
						"severity=%s\n"	\
						"length=%d\n",	\
						ev->timestamp,	\
						ev->severity,	\
						ev->length);

	strcat(buf, bt_buf);

	return 0;
}

static int set_arm_event_backtrace(char *buf, struct ras_arm_event *ev)
{
	char bt_buf[MAX_BACKTRACE_SIZE];

	if (!buf || !ev)
		return -1;

	sprintf(bt_buf, "BACKTRACE="    \
						"timestamp=%s\n"	\
						"error_count=%d\n"	\
						"affinity=%d\n"	\
						"mpidr=0x%lx\n"	\
						"midr=0x%lx\n"	\
						"running_state=%d\n"	\
						"psci_state=%d\n",	\
						ev->timestamp,	\
						ev->error_count,	\
						ev->affinity,	\
						ev->mpidr,	\
						ev->midr,	\
						ev->running_state,	\
						ev->psci_state);

	strcat(buf, bt_buf);

	return 0;
}

static int set_devlink_event_backtrace(char *buf, struct devlink_event *ev)
{
	char bt_buf[MAX_BACKTRACE_SIZE];

	if (!buf || !ev)
		return -1;

	sprintf(bt_buf, "BACKTRACE="	\
						"timestamp=%s\n"	\
						"bus_name=%s\n"		\
						"dev_name=%s\n"		\
						"driver_name=%s\n"	\
						"reporter_name=%s\n"	\
						"msg=%s\n",		\
						ev->timestamp,		\
						ev->bus_name,		\
						ev->dev_name,		\
						ev->driver_name,	\
						ev->reporter_name,	\
						ev->msg);

	strcat(buf, bt_buf);

	return 0;
}

static int set_diskerror_event_backtrace(char *buf, struct diskerror_event *ev)
{
	char bt_buf[MAX_BACKTRACE_SIZE];

	if (!buf || !ev)
		return -1;

	sprintf(bt_buf, "BACKTRACE="	\
						"timestamp=%s\n"	\
						"dev=%s\n"		\
						"sector=%llu\n"		\
						"nr_sector=%u\n"	\
						"error=%s\n"		\
						"rwbs=%s\n"		\
						"cmd=%s\n",		\
						ev->timestamp,		\
						ev->dev,		\
						ev->sector,		\
						ev->nr_sector,		\
						ev->error,		\
						ev->rwbs,		\
						ev->cmd);

	strcat(buf, bt_buf);

	return 0;
}

static int set_mf_event_backtrace(char *buf, struct ras_mf_event *ev)
{
	char bt_buf[MAX_BACKTRACE_SIZE];

	if (!buf || !ev)
		return -1;

	sprintf(bt_buf, "BACKTRACE="    \
						"timestamp=%s\n"	\
						"pfn=%s\n"		\
						"page_type=%s\n"	\
						"action_result=%s\n",	\
						ev->timestamp,		\
						ev->pfn,		\
						ev->page_type,		\
						ev->action_result);

	strcat(buf, bt_buf);

	return 0;
}

static int set_cxl_poison_event_backtrace(char *buf, struct ras_cxl_poison_event *ev)
{
	char bt_buf[MAX_BACKTRACE_SIZE];

	if (!buf || !ev)
		return -1;

	sprintf(bt_buf, "BACKTRACE="	\
						"timestamp=%s\n"	\
						"memdev=%s\n"		\
						"host=%s\n"		\
						"serial=0x%lx\n"	\
						"trace_type=%s\n"	\
						"region=%s\n"		\
						"region_uuid=%s\n"	\
						"hpa=0x%lx\n"		\
						"dpa=0x%lx\n"		\
						"dpa_length=0x%x\n"	\
						"source=%s\n"		\
						"flags=%u\n"		\
						"overflow_timestamp=%s\n", \
						ev->timestamp,		\
						ev->memdev,		\
						ev->host,		\
						ev->serial,		\
						ev->trace_type,		\
						ev->region,		\
						ev->uuid,		\
						ev->hpa,		\
						ev->dpa,		\
						ev->dpa_length,		\
						ev->source,		\
						ev->flags,		\
						ev->overflow_ts);

	strcat(buf, bt_buf);

	return 0;
}

static int set_cxl_aer_ue_event_backtrace(char *buf, struct ras_cxl_aer_ue_event *ev)
{
	char bt_buf[MAX_BACKTRACE_SIZE];

	if (!buf || !ev)
		return -1;

	sprintf(bt_buf, "BACKTRACE="	\
						"timestamp=%s\n"	\
						"memdev=%s\n"		\
						"host=%s\n"		\
						"serial=0x%lx\n"	\
						"error_status=%u\n"	\
						"first_error=%u\n",	\
						ev->timestamp,		\
						ev->memdev,		\
						ev->host,		\
						ev->serial,		\
						ev->error_status,	\
						ev->first_error);

	strcat(buf, bt_buf);

	return 0;
}

static int set_cxl_aer_ce_event_backtrace(char *buf, struct ras_cxl_aer_ce_event *ev)
{
	char bt_buf[MAX_BACKTRACE_SIZE];

	if (!buf || !ev)
		return -1;

	sprintf(bt_buf, "BACKTRACE="	\
						"timestamp=%s\n"	\
						"memdev=%s\n"		\
						"host=%s\n"		\
						"serial=0x%lx\n"	\
						"error_status=%u\n",	\
						ev->timestamp,		\
						ev->memdev,		\
						ev->host,		\
						ev->serial,		\
						ev->error_status);

	strcat(buf, bt_buf);

	return 0;
}

static int set_cxl_overflow_event_backtrace(char *buf, struct ras_cxl_overflow_event *ev)
{
	char bt_buf[MAX_BACKTRACE_SIZE];

	if (!buf || !ev)
		return -1;

	sprintf(bt_buf, "BACKTRACE="	\
						"timestamp=%s\n"	\
						"memdev=%s\n"		\
						"host=%s\n"		\
						"serial=0x%lx\n"	\
						"log_type=%s\n"		\
						"count=%u\n"		\
						"first_ts=%s\n"		\
						"last_ts=%s\n",		\
						ev->timestamp,		\
						ev->memdev,		\
						ev->host,		\
						ev->serial,		\
						ev->log_type,		\
						ev->count,		\
						ev->first_ts,		\
						ev->last_ts);

	strcat(buf, bt_buf);

	return 0;
}

static int set_cxl_generic_event_backtrace(char *buf, struct ras_cxl_generic_event *ev)
{
	char bt_buf[MAX_BACKTRACE_SIZE];

	if (!buf || !ev)
		return -1;

	sprintf(bt_buf, "BACKTRACE="	\
						"timestamp=%s\n"	\
						"memdev=%s\n"		\
						"host=%s\n"		\
						"serial=0x%lx\n"	\
						"log_type=%s\n"		\
						"hdr_uuid=%s\n"		\
						"hdr_flags=0x%x\n"	\
						"hdr_handle=0x%x\n"	\
						"hdr_related_handle=0x%x\n"	\
						"hdr_timestamp=%s\n"	\
						"hdr_length=%u\n"	\
						"hdr_maint_op_class=%u\n",	\
						ev->hdr.timestamp,	\
						ev->hdr.memdev,		\
						ev->hdr.host,		\
						ev->hdr.serial,		\
						ev->hdr.log_type,	\
						ev->hdr.hdr_uuid,	\
						ev->hdr.hdr_flags,	\
						ev->hdr.hdr_handle,	\
						ev->hdr.hdr_related_handle,	\
						ev->hdr.hdr_timestamp,	\
						ev->hdr.hdr_length,	\
						ev->hdr.hdr_maint_op_class);

	strcat(buf, bt_buf);

	return 0;
}

static int set_cxl_general_media_event_backtrace(char *buf, struct ras_cxl_general_media_event *ev)
{
	char bt_buf[MAX_BACKTRACE_SIZE];

	if (!buf || !ev)
		return -1;

	sprintf(bt_buf, "BACKTRACE="	\
						"timestamp=%s\n"	\
						"memdev=%s\n"		\
						"host=%s\n"		\
						"serial=0x%lx\n"	\
						"log_type=%s\n"		\
						"hdr_uuid=%s\n"		\
						"hdr_flags=0x%x\n"	\
						"hdr_handle=0x%x\n"	\
						"hdr_related_handle=0x%x\n"	\
						"hdr_timestamp=%s\n"	\
						"hdr_length=%u\n"	\
						"hdr_maint_op_class=%u\n"	\
						"dpa=0x%lx\n"		\
						"dpa_flags=%u\n"	\
						"descriptor=%u\n"	\
						"type=%u\n"		\
						"transaction_type=%u\n"	\
						"channel=%u\n"		\
						"rank=%u\n"		\
						"device=0x%x\n",	\
						ev->hdr.timestamp,	\
						ev->hdr.memdev,		\
						ev->hdr.host,		\
						ev->hdr.serial,		\
						ev->hdr.log_type,	\
						ev->hdr.hdr_uuid,	\
						ev->hdr.hdr_flags,	\
						ev->hdr.hdr_handle,	\
						ev->hdr.hdr_related_handle,	\
						ev->hdr.hdr_timestamp,	\
						ev->hdr.hdr_length,	\
						ev->hdr.hdr_maint_op_class,	\
						ev->dpa,		\
						ev->dpa_flags,		\
						ev->descriptor,		\
						ev->type,		\
						ev->transaction_type,	\
						ev->channel,		\
						ev->rank,		\
						ev->device);

	strcat(buf, bt_buf);

	return 0;
}

static int set_cxl_dram_event_backtrace(char *buf, struct ras_cxl_dram_event *ev)
{
	char bt_buf[MAX_BACKTRACE_SIZE];

	if (!buf || !ev)
		return -1;

	sprintf(bt_buf, "BACKTRACE="	\
						"timestamp=%s\n"	\
						"memdev=%s\n"		\
						"host=%s\n"		\
						"serial=0x%lx\n"	\
						"log_type=%s\n"		\
						"hdr_uuid=%s\n"		\
						"hdr_flags=0x%x\n"	\
						"hdr_handle=0x%x\n"	\
						"hdr_related_handle=0x%x\n"	\
						"hdr_timestamp=%s\n"	\
						"hdr_length=%u\n"	\
						"hdr_maint_op_class=%u\n"	\
						"dpa=0x%lx\n"		\
						"dpa_flags=%u\n"	\
						"descriptor=%u\n"	\
						"type=%u\n"		\
						"transaction_type=%u\n"	\
						"channel=%u\n"		\
						"rank=%u\n"		\
						"nibble_mask=%u\n"	\
						"bank_group=%u\n"	\
						"bank=%u\n"		\
						"row=%u\n"		\
						"column=%u\n",		\
						ev->hdr.timestamp,	\
						ev->hdr.memdev,		\
						ev->hdr.host,		\
						ev->hdr.serial,		\
						ev->hdr.log_type,	\
						ev->hdr.hdr_uuid,	\
						ev->hdr.hdr_flags,	\
						ev->hdr.hdr_handle,	\
						ev->hdr.hdr_related_handle,	\
						ev->hdr.hdr_timestamp,	\
						ev->hdr.hdr_length,	\
						ev->hdr.hdr_maint_op_class,	\
						ev->dpa,		\
						ev->dpa_flags,		\
						ev->descriptor,		\
						ev->type,		\
						ev->transaction_type,	\
						ev->channel,		\
						ev->rank,		\
						ev->nibble_mask,	\
						ev->bank_group,		\
						ev->bank,		\
						ev->row,		\
						ev->column);

	strcat(buf, bt_buf);

	return 0;
}

static int set_cxl_memory_module_event_backtrace(char *buf, struct ras_cxl_memory_module_event *ev)
{
	char bt_buf[MAX_BACKTRACE_SIZE];

	if (!buf || !ev)
		return -1;

	sprintf(bt_buf, "BACKTRACE="	\
						"timestamp=%s\n"	\
						"memdev=%s\n"		\
						"host=%s\n"		\
						"serial=0x%lx\n"	\
						"log_type=%s\n"		\
						"hdr_uuid=%s\n"		\
						"hdr_flags=0x%x\n"	\
						"hdr_handle=0x%x\n"	\
						"hdr_related_handle=0x%x\n"	\
						"hdr_timestamp=%s\n"	\
						"hdr_length=%u\n"	\
						"hdr_maint_op_class=%u\n"	\
						"event_type=%u\n"	\
						"health_status=%u\n"	\
						"media_status=%u\n"	\
						"life_used=%u\n"	\
						"dirty_shutdown_cnt=%u\n"	\
						"cor_vol_err_cnt=%u\n"	\
						"cor_per_err_cnt=%u\n"	\
						"device_temp=%d\n"	\
						"add_status=%u\n",	\
						ev->hdr.timestamp,	\
						ev->hdr.memdev,		\
						ev->hdr.host,		\
						ev->hdr.serial,		\
						ev->hdr.log_type,	\
						ev->hdr.hdr_uuid,	\
						ev->hdr.hdr_flags,	\
						ev->hdr.hdr_handle,	\
						ev->hdr.hdr_related_handle,	\
						ev->hdr.hdr_timestamp,	\
						ev->hdr.hdr_length,	\
						ev->hdr.hdr_maint_op_class,	\
						ev->event_type,		\
						ev->health_status,	\
						ev->media_status,	\
						ev->life_used,		\
						ev->dirty_shutdown_cnt,	\
						ev->cor_vol_err_cnt,	\
						ev->cor_per_err_cnt,	\
						ev->device_temp,	\
						ev->add_status);

	strcat(buf, bt_buf);

	return 0;
}

static int commit_report_backtrace(int sockfd, int type, void *ev)
{
	char buf[MAX_BACKTRACE_SIZE];
	char *pbuf = buf;
	int rc = -1;
	int buf_len = 0;

	if (sockfd < 0 || !ev) {
		return -1;
	}

	memset(buf, 0, MAX_BACKTRACE_SIZE);

	switch (type) {
	case MC_EVENT:
		rc = set_mc_event_backtrace(buf, (struct ras_mc_event *)ev);
		break;
	case AER_EVENT:
		rc = set_aer_event_backtrace(buf, (struct ras_aer_event *)ev);
		break;
	case MCE_EVENT:
		rc = set_mce_event_backtrace(buf, (struct mce_event *)ev);
		break;
	case NON_STANDARD_EVENT:
		rc = set_non_standard_event_backtrace(buf, (struct ras_non_standard_event *)ev);
		break;
	case ARM_EVENT:
		rc = set_arm_event_backtrace(buf, (struct ras_arm_event *)ev);
		break;
	case DEVLINK_EVENT:
		rc = set_devlink_event_backtrace(buf, (struct devlink_event *)ev);
		break;
	case DISKERROR_EVENT:
		rc = set_diskerror_event_backtrace(buf, (struct diskerror_event *)ev);
		break;
	case MF_EVENT:
		rc = set_mf_event_backtrace(buf, (struct ras_mf_event *)ev);
		break;
	case CXL_POISON_EVENT:
		rc = set_cxl_poison_event_backtrace(buf, (struct ras_cxl_poison_event *)ev);
		break;
	case CXL_AER_UE_EVENT:
		rc = set_cxl_aer_ue_event_backtrace(buf, (struct ras_cxl_aer_ue_event *)ev);
		break;
	case CXL_AER_CE_EVENT:
		rc = set_cxl_aer_ce_event_backtrace(buf, (struct ras_cxl_aer_ce_event *)ev);
		break;
	case CXL_OVERFLOW_EVENT:
		rc = set_cxl_overflow_event_backtrace(buf, (struct ras_cxl_overflow_event *)ev);
		break;
	case CXL_GENERIC_EVENT:
		rc = set_cxl_generic_event_backtrace(buf, (struct ras_cxl_generic_event *)ev);
		break;
	case CXL_GENERAL_MEDIA_EVENT:
		rc = set_cxl_general_media_event_backtrace(buf, (struct ras_cxl_general_media_event *)ev);
		break;
	case CXL_DRAM_EVENT:
		rc = set_cxl_dram_event_backtrace(buf, (struct ras_cxl_dram_event *)ev);
		break;
	case CXL_MEMORY_MODULE_EVENT:
		rc = set_cxl_memory_module_event_backtrace(buf, (struct ras_cxl_memory_module_event *)ev);
		break;
	default:
		return -1;
	}

	if (rc < 0) {
		return -1;
	}

	buf_len = strlen(buf);

	for (; buf_len > INPUT_BUFFER_SIZE - 1; buf_len -= (INPUT_BUFFER_SIZE - 1)) {
		rc = write(sockfd, pbuf, INPUT_BUFFER_SIZE - 1);
		if (rc < INPUT_BUFFER_SIZE - 1) {
			return -1;
		}

		pbuf = pbuf + INPUT_BUFFER_SIZE - 1;
	}

	rc = write(sockfd, pbuf, buf_len + 1);
	if (rc < buf_len) {
		return -1;
	}

	return 0;
}

int ras_report_mc_event(struct ras_events *ras, struct ras_mc_event *ev)
{
	char buf[MAX_MESSAGE_SIZE];
	int sockfd = -1;
	int done = 0;
	int rc = -1;

	memset(buf, 0, sizeof(buf));

	sockfd = setup_report_socket();
	if (sockfd < 0) {
		return -1;
	}

	rc = commit_report_basic(sockfd);
	if (rc < 0) {
		goto mc_fail;
	}

	rc = commit_report_backtrace(sockfd, MC_EVENT, ev);
	if (rc < 0) {
		goto mc_fail;
	}

	sprintf(buf, "ANALYZER=%s", "rasdaemon-mc");
	rc = write(sockfd, buf, strlen(buf) + 1);
	if (rc < strlen(buf) + 1) {
		goto mc_fail;
	}

	sprintf(buf, "REASON=%s", "EDAC driver report problem");
	rc = write(sockfd, buf, strlen(buf) + 1);
	if (rc < strlen(buf) + 1) {
		goto mc_fail;
	}

	done = 1;

mc_fail:

	if (sockfd >= 0) {
		close(sockfd);
	}

	if (done) {
		return 0;
	} else {
		return -1;
	}
}

int ras_report_aer_event(struct ras_events *ras, struct ras_aer_event *ev)
{
	char buf[MAX_MESSAGE_SIZE];
	int sockfd = 0;
	int done = 0;
	int rc = -1;

	memset(buf, 0, sizeof(buf));

	sockfd = setup_report_socket();
	if (sockfd < 0) {
		return -1;
	}

	rc = commit_report_basic(sockfd);
	if (rc < 0) {
		goto aer_fail;
	}

	rc = commit_report_backtrace(sockfd, AER_EVENT, ev);
	if (rc < 0) {
		goto aer_fail;
	}

	sprintf(buf, "ANALYZER=%s", "rasdaemon-aer");
	rc = write(sockfd, buf, strlen(buf) + 1);
	if (rc < strlen(buf) + 1) {
		goto aer_fail;
	}

	sprintf(buf, "REASON=%s", "PCIe AER driver report problem");
	rc = write(sockfd, buf, strlen(buf) + 1);
	if (rc < strlen(buf) + 1) {
		goto aer_fail;
	}

	done = 1;

aer_fail:

	if (sockfd >= 0) {
		close(sockfd);
	}

	if (done) {
		return 0;
	} else {
		return -1;
	}
}

int ras_report_non_standard_event(struct ras_events *ras, struct ras_non_standard_event *ev)
{
	char buf[MAX_MESSAGE_SIZE];
	int sockfd = 0;
	int rc = -1;

	memset(buf, 0, sizeof(buf));

	sockfd = setup_report_socket();
	if (sockfd < 0) {
		return rc;
	}

	rc = commit_report_basic(sockfd);
	if (rc < 0) {
		goto non_standard_fail;
	}

	rc = commit_report_backtrace(sockfd, NON_STANDARD_EVENT, ev);
	if (rc < 0) {
		goto non_standard_fail;
	}

	sprintf(buf, "ANALYZER=%s", "rasdaemon-non-standard");
	rc = write(sockfd, buf, strlen(buf) + 1);
	if (rc < strlen(buf) + 1) {
		goto non_standard_fail;
	}

	sprintf(buf, "REASON=%s", "Unknown CPER section problem");
	rc = write(sockfd, buf, strlen(buf) + 1);
	if (rc < strlen(buf) + 1) {
		goto non_standard_fail;
	}

	rc = 0;

non_standard_fail:

	if (sockfd >= 0) {
		close(sockfd);
	}

	return rc;
}

int ras_report_arm_event(struct ras_events *ras, struct ras_arm_event *ev)
{
	char buf[MAX_MESSAGE_SIZE];
	int sockfd = 0;
	int rc = -1;

	memset(buf, 0, sizeof(buf));

	sockfd = setup_report_socket();
	if (sockfd < 0) {
		return rc;
	}

	rc = commit_report_basic(sockfd);
	if (rc < 0) {
		goto arm_fail;
	}

	rc = commit_report_backtrace(sockfd, ARM_EVENT, ev);
	if (rc < 0) {
		goto arm_fail;
	}

	sprintf(buf, "ANALYZER=%s", "rasdaemon-arm");
	rc = write(sockfd, buf, strlen(buf) + 1);
	if (rc < strlen(buf) + 1) {
		goto arm_fail;
	}

	sprintf(buf, "REASON=%s", "ARM CPU report problem");
	rc = write(sockfd, buf, strlen(buf) + 1);
	if (rc < strlen(buf) + 1) {
		goto arm_fail;
	}

	rc = 0;

arm_fail:

	if (sockfd >= 0) {
		close(sockfd);
	}

	return rc;
}

int ras_report_mce_event(struct ras_events *ras, struct mce_event *ev)
{
	char buf[MAX_MESSAGE_SIZE];
	int sockfd = 0;
	int done = 0;
	int rc = -1;

	memset(buf, 0, sizeof(buf));

	sockfd = setup_report_socket();
	if (sockfd < 0) {
		return -1;
	}

	rc = commit_report_basic(sockfd);
	if (rc < 0) {
		goto mce_fail;
	}

	rc = commit_report_backtrace(sockfd, MCE_EVENT, ev);
	if (rc < 0) {
		goto mce_fail;
	}

	sprintf(buf, "ANALYZER=%s", "rasdaemon-mce");
	rc = write(sockfd, buf, strlen(buf) + 1);
	if (rc < strlen(buf) + 1) {
		goto mce_fail;
	}

	sprintf(buf, "REASON=%s", "Machine Check driver report problem");
	rc = write(sockfd, buf, strlen(buf) + 1);
	if (rc < strlen(buf) + 1) {
		goto mce_fail;
	}

	done = 1;

mce_fail:

	if (sockfd >= 0) {
		close(sockfd);
	}

	if (done) {
		return 0;
	} else {
		return -1;
	}
}

int ras_report_devlink_event(struct ras_events *ras, struct devlink_event *ev)
{
	char buf[MAX_MESSAGE_SIZE];
	int sockfd = 0;
	int done = 0;
	int rc = -1;

	memset(buf, 0, sizeof(buf));

	sockfd = setup_report_socket();
	if (sockfd < 0) {
		return -1;
	}

	rc = commit_report_basic(sockfd);
	if (rc < 0) {
		goto devlink_fail;
	}

	rc = commit_report_backtrace(sockfd, DEVLINK_EVENT, ev);
	if (rc < 0) {
		goto devlink_fail;
	}

	sprintf(buf, "ANALYZER=%s", "rasdaemon-devlink");
	rc = write(sockfd, buf, strlen(buf) + 1);
	if (rc < strlen(buf) + 1) {
		goto devlink_fail;
	}

	sprintf(buf, "REASON=%s", "devlink health report problem");
	rc = write(sockfd, buf, strlen(buf) + 1);
	if (rc < strlen(buf) + 1) {
		goto devlink_fail;
	}

	done = 1;

devlink_fail:

	if (sockfd >= 0) {
		close(sockfd);
	}

	if (done) {
		return 0;
	} else {
		return -1;
	}
}

int ras_report_diskerror_event(struct ras_events *ras, struct diskerror_event *ev)
{
	char buf[MAX_MESSAGE_SIZE];
	int sockfd = 0;
	int done = 0;
	int rc = -1;

	memset(buf, 0, sizeof(buf));

	sockfd = setup_report_socket();
	if (sockfd < 0) {
		return -1;
	}

	rc = commit_report_basic(sockfd);
	if (rc < 0) {
		goto diskerror_fail;
	}

	rc = commit_report_backtrace(sockfd, DISKERROR_EVENT, ev);
	if (rc < 0) {
		goto diskerror_fail;
	}

	sprintf(buf, "ANALYZER=%s", "rasdaemon-diskerror");
	rc = write(sockfd, buf, strlen(buf) + 1);
	if (rc < strlen(buf) + 1) {
		goto diskerror_fail;
	}

	sprintf(buf, "REASON=%s", "disk I/O error");
	rc = write(sockfd, buf, strlen(buf) + 1);
	if (rc < strlen(buf) + 1) {
		goto diskerror_fail;
	}

	done = 1;

diskerror_fail:
	if (sockfd >= 0) {
		close(sockfd);
	}

	if (done) {
		return 0;
	} else {
		return -1;
	}
}

int ras_report_mf_event(struct ras_events *ras, struct ras_mf_event *ev)
{
	char buf[MAX_MESSAGE_SIZE];
	int sockfd = 0;
	int done = 0;
	int rc = -1;

	memset(buf, 0, sizeof(buf));

	sockfd = setup_report_socket();
	if (sockfd < 0)
		return -1;

	rc = commit_report_basic(sockfd);
	if (rc < 0)
		goto mf_fail;

	rc = commit_report_backtrace(sockfd, MF_EVENT, ev);
	if (rc < 0)
		goto mf_fail;

	sprintf(buf, "ANALYZER=%s", "rasdaemon-memory_failure");
	rc = write(sockfd, buf, strlen(buf) + 1);
	if (rc < strlen(buf) + 1)
		goto mf_fail;

	sprintf(buf, "REASON=%s", "memory failure problem");
	rc = write(sockfd, buf, strlen(buf) + 1);
	if (rc < strlen(buf) + 1)
		goto mf_fail;

	done = 1;

mf_fail:
	if (sockfd >= 0)
		close(sockfd);

	if (done)
		return 0;
	else
		return -1;
}

int ras_report_cxl_poison_event(struct ras_events *ras, struct ras_cxl_poison_event *ev)
{
	char buf[MAX_MESSAGE_SIZE];
	int sockfd = 0;
	int done = 0;
	int rc = -1;

	memset(buf, 0, sizeof(buf));

	sockfd = setup_report_socket();
	if (sockfd < 0)
		return -1;

	rc = commit_report_basic(sockfd);
	if (rc < 0)
		goto cxl_poison_fail;

	rc = commit_report_backtrace(sockfd, CXL_POISON_EVENT, ev);
	if (rc < 0)
		goto cxl_poison_fail;

	sprintf(buf, "ANALYZER=%s", "rasdaemon-cxl-poison");
	rc = write(sockfd, buf, strlen(buf) + 1);
	if (rc < strlen(buf) + 1)
		goto cxl_poison_fail;

	sprintf(buf, "REASON=%s", "CXL poison");
	rc = write(sockfd, buf, strlen(buf) + 1);
	if (rc < strlen(buf) + 1)
		goto cxl_poison_fail;

	done = 1;

cxl_poison_fail:

	if (sockfd >= 0)
		close(sockfd);

	if (done)
		return 0;
	else
		return -1;
}

int ras_report_cxl_aer_ue_event(struct ras_events *ras, struct ras_cxl_aer_ue_event *ev)
{
	char buf[MAX_MESSAGE_SIZE];
	int sockfd = 0;
	int done = 0;
	int rc = -1;

	memset(buf, 0, sizeof(buf));

	sockfd = setup_report_socket();
	if (sockfd < 0)
		return -1;

	rc = commit_report_basic(sockfd);
	if (rc < 0)
		goto cxl_aer_ue_fail;

	rc = commit_report_backtrace(sockfd, CXL_AER_UE_EVENT, ev);
	if (rc < 0)
		goto cxl_aer_ue_fail;

	sprintf(buf, "ANALYZER=%s", "rasdaemon-cxl-aer-uncorrectable-error");
	rc = write(sockfd, buf, strlen(buf) + 1);
	if (rc < strlen(buf) + 1)
		goto cxl_aer_ue_fail;

	sprintf(buf, "REASON=%s", "CXL AER uncorrectable error");
	rc = write(sockfd, buf, strlen(buf) + 1);
	if (rc < strlen(buf) + 1)
		goto cxl_aer_ue_fail;

	done = 1;

cxl_aer_ue_fail:

	if (sockfd >= 0)
		close(sockfd);

	if (done)
		return 0;
	else
		return -1;
}

int ras_report_cxl_aer_ce_event(struct ras_events *ras, struct ras_cxl_aer_ce_event *ev)
{
	char buf[MAX_MESSAGE_SIZE];
	int sockfd = 0;
	int done = 0;
	int rc = -1;

	memset(buf, 0, sizeof(buf));

	sockfd = setup_report_socket();
	if (sockfd < 0)
		return -1;

	rc = commit_report_basic(sockfd);
	if (rc < 0)
		goto cxl_aer_ce_fail;

	rc = commit_report_backtrace(sockfd, CXL_AER_CE_EVENT, ev);
	if (rc < 0)
		goto cxl_aer_ce_fail;

	sprintf(buf, "ANALYZER=%s", "rasdaemon-cxl-aer-correctable-error");
	rc = write(sockfd, buf, strlen(buf) + 1);
	if (rc < strlen(buf) + 1)
		goto cxl_aer_ce_fail;

	sprintf(buf, "REASON=%s", "CXL AER correctable error");
	rc = write(sockfd, buf, strlen(buf) + 1);
	if (rc < strlen(buf) + 1)
		goto cxl_aer_ce_fail;

	done = 1;

cxl_aer_ce_fail:

	if (sockfd >= 0)
		close(sockfd);

	if (done)
		return 0;
	else
		return -1;
}

int ras_report_cxl_overflow_event(struct ras_events *ras, struct ras_cxl_overflow_event *ev)
{
	char buf[MAX_MESSAGE_SIZE];
	int sockfd = 0;
	int done = 0;
	int rc = -1;

	memset(buf, 0, sizeof(buf));

	sockfd = setup_report_socket();
	if (sockfd < 0)
		return -1;

	rc = commit_report_basic(sockfd);
	if (rc < 0)
		goto cxl_overflow_fail;

	rc = commit_report_backtrace(sockfd, CXL_OVERFLOW_EVENT, ev);
	if (rc < 0)
		goto cxl_overflow_fail;

	sprintf(buf, "ANALYZER=%s", "rasdaemon-cxl-overflow");
	rc = write(sockfd, buf, strlen(buf) + 1);
	if (rc < strlen(buf) + 1)
		goto cxl_overflow_fail;

	sprintf(buf, "REASON=%s", "CXL overflow");
	rc = write(sockfd, buf, strlen(buf) + 1);
	if (rc < strlen(buf) + 1)
		goto cxl_overflow_fail;

	done = 1;

cxl_overflow_fail:

	if (sockfd >= 0)
		close(sockfd);

	if (done)
		return 0;
	else
		return -1;
}

int ras_report_cxl_generic_event(struct ras_events *ras, struct ras_cxl_generic_event *ev)
{
	char buf[MAX_MESSAGE_SIZE];
	int sockfd = 0;
	int done = 0;
	int rc = -1;

	memset(buf, 0, sizeof(buf));

	sockfd = setup_report_socket();
	if (sockfd < 0)
		return -1;

	rc = commit_report_basic(sockfd);
	if (rc < 0)
		goto cxl_generic_fail;

	rc = commit_report_backtrace(sockfd, CXL_GENERIC_EVENT, ev);
	if (rc < 0)
		goto cxl_generic_fail;

	sprintf(buf, "ANALYZER=%s", "rasdaemon-cxl_generic_event");
	rc = write(sockfd, buf, strlen(buf) + 1);
	if (rc < strlen(buf) + 1)
		goto cxl_generic_fail;

	sprintf(buf, "REASON=%s", "CXL Generic Event ");
	rc = write(sockfd, buf, strlen(buf) + 1);
	if (rc < strlen(buf) + 1)
		goto cxl_generic_fail;

	done = 1;

cxl_generic_fail:

	if (sockfd >= 0)
		close(sockfd);

	if (done)
		return 0;
	else
		return -1;
}

int ras_report_cxl_general_media_event(struct ras_events *ras, struct ras_cxl_general_media_event *ev)
{
	char buf[MAX_MESSAGE_SIZE];
	int sockfd = 0;
	int done = 0;
	int rc = -1;

	memset(buf, 0, sizeof(buf));

	sockfd = setup_report_socket();
	if (sockfd < 0)
		return -1;

	rc = commit_report_basic(sockfd);
	if (rc < 0)
		goto cxl_general_media_fail;

	rc = commit_report_backtrace(sockfd, CXL_GENERAL_MEDIA_EVENT, ev);
	if (rc < 0)
		goto cxl_general_media_fail;

	sprintf(buf, "ANALYZER=%s", "rasdaemon-cxl_general_media_event");
	rc = write(sockfd, buf, strlen(buf) + 1);
	if (rc < strlen(buf) + 1)
		goto cxl_general_media_fail;

	sprintf(buf, "REASON=%s", "CXL General Media Event");
	rc = write(sockfd, buf, strlen(buf) + 1);
	if (rc < strlen(buf) + 1)
		goto cxl_general_media_fail;

	done = 1;

cxl_general_media_fail:

	if (sockfd >= 0)
		close(sockfd);

	if (done)
		return 0;
	else
		return -1;
}

int ras_report_cxl_dram_event(struct ras_events *ras, struct ras_cxl_dram_event *ev)
{
	char buf[MAX_MESSAGE_SIZE];
	int sockfd = 0;
	int done = 0;
	int rc = -1;

	memset(buf, 0, sizeof(buf));

	sockfd = setup_report_socket();
	if (sockfd < 0)
		return -1;

	rc = commit_report_basic(sockfd);
	if (rc < 0)
		goto cxl_dram_fail;

	rc = commit_report_backtrace(sockfd, CXL_DRAM_EVENT, ev);
	if (rc < 0)
		goto cxl_dram_fail;

	sprintf(buf, "ANALYZER=%s", "rasdaemon-cxl_dram_event");
	rc = write(sockfd, buf, strlen(buf) + 1);
	if (rc < strlen(buf) + 1)
		goto cxl_dram_fail;

	sprintf(buf, "REASON=%s", "CXL DRAM Event");
	rc = write(sockfd, buf, strlen(buf) + 1);
	if (rc < strlen(buf) + 1)
		goto cxl_dram_fail;

	done = 1;

cxl_dram_fail:

	if (sockfd >= 0)
		close(sockfd);

	if (done)
		return 0;
	else
		return -1;
}

int ras_report_cxl_memory_module_event(struct ras_events *ras, struct ras_cxl_memory_module_event *ev)
{
	char buf[MAX_MESSAGE_SIZE];
	int sockfd = 0;
	int done = 0;
	int rc = -1;

	memset(buf, 0, sizeof(buf));

	sockfd = setup_report_socket();
	if (sockfd < 0)
		return -1;

	rc = commit_report_basic(sockfd);
	if (rc < 0)
		goto cxl_memory_module_fail;

	rc = commit_report_backtrace(sockfd, CXL_MEMORY_MODULE_EVENT, ev);
	if (rc < 0)
		goto cxl_memory_module_fail;

	sprintf(buf, "ANALYZER=%s", "rasdaemon-cxl_memory_module_event");
	rc = write(sockfd, buf, strlen(buf) + 1);
	if (rc < strlen(buf) + 1)
		goto cxl_memory_module_fail;

	sprintf(buf, "REASON=%s", "CXL Memory Module Event");
	rc = write(sockfd, buf, strlen(buf) + 1);
	if (rc < strlen(buf) + 1)
		goto cxl_memory_module_fail;

	done = 1;

cxl_memory_module_fail:

	if (sockfd >= 0)
		close(sockfd);

	if (done)
		return 0;
	else
		return -1;
}
