/*
 * Copyright (c) 2023, Meta Platforms Inc.
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
#include <stdbool.h>
#include "ras-record.h"
#include "ras-logger.h"
#include "ras-report.h"
#include "unified-sel.h"

/* CPU Root Port Error ID corresponding to each status bit set */
static const char *cor_error_ids[32] = {
  /* Correctable errors */
  [0]  = "0x00", /* Receiver Error */
  [6]  = "0x01", /* Bad TLP */
  [7]  = "0x02", /* Bad DLLP */
  [8]  = "0x04", /* RELAY_NUM Rollover */
  [12] = "0x03", /* Replay Timer Timeout */
  [13] = "0x05", /* Advisory Non-Fatal */
  [14] = "0x06", /* Corrected Internal */
  [15] = "0x07", /* Header Log Overflow */
};

static const char *uncor_error_ids[32] = {
  /* Uncorrectable errors */
  [4]  = "0x20", /* Data Link Protocol */
  [5]  = "0x21", /* Surprise Link Down */
  [12] = "0x22", /* Poisoned TLP */
  [13] = "0x23", /* Flow Control Protocol */
  [14] = "0x24", /* Completion Timeout */
  [15] = "0x25", /* Completer Abort */
  [16] = "0x26", /* Unexpected Completion */
  [17] = "0x27", /* Receiver Overflow */
  [18] = "0x29", /* Malformed TLP */
  [19] = "0x29", /* ECRC */
  [20] = "0x2A", /* Unsupported Request */
  [21] = "0x2B", /* ACS Violation */
  [22] = "0x2C", /* Uncorrected Internal */
  [23] = "0x2D", /* MC Blocked TLP */
  [24] = "0x2E", /* AtomicOp Egress Blocked */
  [25] = "0x2F", /* TLP Prefix Blocked */
  [26] = "0x30", /* Poisoned TLP Egrees Blocked */
};

static int verify_id_log_sel(uint64_t status,
    const char **idarray,
    unsigned bus,
    unsigned dev_fn)
{
  int i;
  char openbmc_ipmi_add_sel[105];

  /*
   * Get PCIe AER error source bus/dev/fn and save it to the BMC SEL
   * as a OpenBMC unified SEL record type.
   * The IPMI command and record fields are defined in IPMI Specification v2.0 (IPMI Spec)
   * ipmitool raw 0x0a 0x44 is "Add SEL Entry Command" defined in IPMI spec chapter 31.6
   * The 16 byte that follow form the SEL Record
   * defined in IPMI spec chapter 32.1 "SEL Event Records"
   * Byte 1~2 are Record ID = 0x00 0x00, unused
   * Byte 3 is Record Type = 0xFB, OEM non-timestamped record type for OpenBMC unified SEL
   * Byte 4~16 are OEM defined
   * Byte 11:
     * Byte11[7:3] Device#
     * Byte11[2:0] Function#
   * Byte 12: Bus number
   * Byte 13-15: Reserved
   * Byte 16: ID of the error detected on the PCle device that triggered this SEL record
   */

  /* Potentially all error status bits could be set for a given PCIe device.
   * Therefore, iterate over all 32 bits each of cor and uncor errors
   */
  for (i = 0; i < 32; i++) {
    if ((status & (1 << i)) && idarray[i]) {
      sprintf(openbmc_ipmi_add_sel,
          "ipmitool raw 0x0a 0x44 0x00 0x00 0xFB 0x20 0x00 0x00 0x00 0x00 0x01 0x00 0x%02x 0x%02x 0x01 0x00 0xff %s",
          dev_fn, bus, idarray[i]);
      if (system(openbmc_ipmi_add_sel) != 0)
        return -1;
    }
  }
  return 0;
}

int openbmc_unified_sel_log(uint64_t severity, const char *dev_name, uint64_t status)
{
  int bus, dev, dev_fn, fn;

  sscanf(dev_name, "%*x:%x:%x.%x", &bus, &dev, &fn);
  dev_fn = (((dev & 0x1f) << 3) | (fn & 0x7));

  /* Use the appropriate correctable error status ID
   * for a given severity level
   */
  if (severity == HW_EVENT_AER_CORRECTED) {
      if (verify_id_log_sel(status, cor_error_ids, bus, dev_fn) < 0)
        return -1;
  }
  return 0;
}
