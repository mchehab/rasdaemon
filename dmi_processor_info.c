// SPDX-License-Identifier: GPL-2.0
/*
 * Code adapted from util-linux/sys-utils/lscpu-dmi.c lscpu.h lscpu-virt.c
 * Copyright (C) 2020 FUJITSU LIMITED.  All rights reserved.
 */

#include <errno.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include "dmi_processor_info.h"

#define _PATH_SYS_DMI           "/sys/firmware/dmi/tables/DMI"

struct lscpu_cputype {
	char    *bios_vendor;
	char    *bios_modelname;
	char    *bios_family;
};

struct lscpu_dmi_header
{
	uint8_t type;
	uint8_t length;
	uint16_t handle;
	uint8_t *data;
};

#define MIN_DMI_HEADER_LEN 4

struct dmi_info {
	char *vendor;
	char *product;
	char *manufacturer;
	int sockets;

	/* Processor Information */
	uint16_t processor_family;
	char *processor_manufacturer;
	char *processor_version;
	uint16_t current_speed;
	char *part_num;
};

static void to_dmi_header(struct lscpu_dmi_header *h, uint8_t *data)
{
	h->type = data[0];
	h->length = data[1];
	memcpy(&h->handle, data + 2, sizeof(h->handle));
	h->data = data;
}

static char *dmi_string(const struct lscpu_dmi_header *dm, uint8_t s)
{
	char *bp = (char *)dm->data;

	if (!s || !bp)
		return NULL;

	bp += dm->length;
	while (s > 1 && *bp) {
		bp += strlen(bp);
		bp++;
		s--;
	}

	return !*bp ? NULL : bp;
}

static int parse_dmi_table(uint16_t len, uint16_t num,
				uint8_t *data,
				struct dmi_info *di)
{
	uint8_t *buf = data;
	int rc = -1;
	int i = 0;

	while (i < num && data + MIN_DMI_HEADER_LEN <= buf + len) {
		uint8_t *next;
		struct lscpu_dmi_header h;

		to_dmi_header(&h, data);

		/*
		 * If a short entry is found (less than 4 bytes), not only it
		 * is invalid, but we cannot reliably locate the next entry.
		 * Better stop at this point.
		 */
		if (h.length < MIN_DMI_HEADER_LEN)
			goto done;

		/*
		 * Look for the next handle, skipping the possible strings section,
		 * ending with 2 null bytes (even when empty).
		 */
		next = data + h.length;
		while (next - buf + 1 < len && (next[0] != 0 || next[1] != 0))
			next++;
		next += 2;

		/*
		 * Types and data offsets are defined in the System Management BIOS
		 * (SMBIOS) Reference specification.
		 */
		switch (h.type) {
		case 0:
			di->vendor = dmi_string(&h, data[0x04]);
			break;
		case 1:
			di->manufacturer = dmi_string(&h, data[0x04]);
			di->product = dmi_string(&h, data[0x05]);
			break;
		case 4:
			/* Get the first processor information */
			if (di->sockets == 0) {
				di->processor_manufacturer = dmi_string(&h, data[0x7]);
				di->processor_version = dmi_string(&h, data[0x10]);
				di->current_speed = *((uint16_t *)(&data[0x16]));
				di->part_num = dmi_string(&h, data[0x22]);

				if (data[0x6] == 0xfe)
					di->processor_family = *((uint16_t *)(&data[0x28]));
				else
					di->processor_family = data[0x6];
			}
			di->sockets++;
			break;
		default:
			break;
		}

		data = next;
		i++;
	}
	rc = 0;
done:
	return rc;
}

static inline ssize_t read_all(int fd, char *buf, size_t count)
{
	ssize_t ret;
	ssize_t c = 0;
	int tries = 0;

	memset(buf, 0, count);
	while (count > 0) {
		ret = read(fd, buf, count);
		if (ret < 0) {
			if ((errno == EAGAIN || errno == EINTR) && (tries++ < 5)) {
				usleep(250000);
				continue;
			}
			return c ? c : -1;
		}
		if (ret == 0)
			return c;
		tries = 0;
		count -= ret;
		buf += ret;
		c += ret;
	}
	return c;
}

static void *get_mem_chunk(size_t base, size_t len, const char *devmem)
{
	void *p = NULL;
	int fd;

	if ((fd = open(devmem, O_RDONLY)) < 0)
		return NULL;

	if (!(p = malloc(len)))
		goto nothing;
	if (lseek(fd, base, SEEK_SET) == -1)
		goto nothing;
	if (read_all(fd, p, len) == -1)
		goto nothing;

	close(fd);
	return p;

nothing:
	free(p);
	close(fd);
	return NULL;
}

static int dmi_decode_cputype(struct lscpu_cputype *ct)
{
	static char const sys_fw_dmi_tables[] = _PATH_SYS_DMI;
	struct dmi_info di = { };
	struct stat st;
	uint8_t *data;
	int rc = -1;
	char buf[100] = { };

	if (stat(sys_fw_dmi_tables, &st))
		return rc;

	data = get_mem_chunk(0, st.st_size, sys_fw_dmi_tables);
	if (!data)
		return rc;

	rc = parse_dmi_table(st.st_size, st.st_size/4, data, &di);
	if (rc < 0) {
		free(data);
		return rc;
	}

	if (di.processor_manufacturer)
		ct->bios_vendor = strdup(di.processor_manufacturer);

	snprintf(buf, sizeof(buf), "%s %s CPU @ %d.%dGHz",
			(di.processor_version ?: ""), (di.part_num ?: ""),
			di.current_speed/1000, (di.current_speed % 1000) / 100);
	ct->bios_modelname = strdup(buf);

	/* Get CPU family */
	memset(buf, 0, sizeof(buf));
	snprintf(buf, sizeof(buf), "%d", di.processor_family);
	ct->bios_family = strdup(buf);

	free(data);
	return 0;
}


#define AMPERE_PREFIX "Ampere(R) Altra(R)"
int is_ampere_altra(void)
{
	int rc = 0;
	struct lscpu_cputype c;
	c.bios_vendor = NULL;
	c.bios_modelname = NULL;
	c.bios_family = NULL;

	if (dmi_decode_cputype(&c) != 0)
		return 0;

	/*
	 * This depends on BIOS implementation to provide the CPU information.
	 * If the BIOS doesn't provide it or gives a different string, the
	 * ipmitool use will be disabled.
	 */
	if (strncmp(c.bios_modelname, AMPERE_PREFIX, strlen(AMPERE_PREFIX)) == 0)
		rc = 1;

	if (c.bios_vendor)
		free(c.bios_vendor);
	if (c.bios_modelname)
		free(c.bios_modelname);
	if (c.bios_family)
		free(c.bios_family);
	return rc;
}
