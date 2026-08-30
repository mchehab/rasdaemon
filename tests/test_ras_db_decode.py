# SPDX-License-Identifier: GPL-2.0-only
# Copyright (C) 2026 Mauro Carvalho Chehab <mchehab+huawei@kernel.org>

import pathlib
import struct
import sys
import unittest
import uuid


UTIL_DIR = pathlib.Path(__file__).resolve().parents[1] / "util"
sys.path.insert(0, str(UTIL_DIR))

from ras_db_decode import format_event_value  # noqa: E402


class RasDatabaseDecodeTest(unittest.TestCase):
    def test_extlog_enums_and_little_endian_uuid(self):
        identifier = uuid.UUID("00112233-4455-6677-8899-aabbccddeeff")

        self.assertEqual(
            format_event_value("extlog_event", "etype", 2),
            "2 (single-bit ECC)",
        )
        self.assertEqual(
            format_event_value("extlog_event", "severity", 3),
            "3 (informational)",
        )
        self.assertEqual(
            format_event_value(
                "extlog_event", "fru_id", identifier.bytes_le
            ),
            str(identifier),
        )

    def test_extlog_cper_memory_payload(self):
        validation = 0x0008 | 0x0100 | 0x0800 | 0x8000
        payload = struct.pack(
            "<Q8H3Q3H", validation,
            2, 0, 0, 0, 0, 37, 0, 0,
            0x1234, 0, 0,
            1, 0, 0,
        )

        self.assertEqual(
            format_event_value("extlog_event", "cper_data", payload),
            "node=2, row=37, requestor_id=0x1234, rank=1",
        )

    def test_cxl_status_flags_and_enums(self):
        self.assertEqual(
            format_event_value("cxl_aer_ue_event", "error_status", 0x5),
            "0x5 (Cache Data Parity Error | Cache Byte Enable Parity Error)",
        )
        self.assertEqual(
            format_event_value("cxl_general_media_event", "type", 4),
            "4 (Scrub Media ECC Error)",
        )
        self.assertEqual(
            format_event_value("cxl_memory_sparing_event", "flags", 0x6),
            "0x6 (HARD_SPARING | DEVICE_INITIATED)",
        )

    def test_cxl_header_log_uses_stored_big_endian_words(self):
        self.assertEqual(
            format_event_value(
                "cxl_aer_ue_event", "header_log",
                bytes.fromhex("01020304 aabbccdd"),
            ),
            "01020304 aabbccdd",
        )

    def test_nvidia_register_pairs(self):
        payload = struct.pack("<QQ", 0x1234, 0x5678)

        self.assertEqual(
            format_event_value("nvidia_ns_event", "reg_data", payload),
            "Reg[0]: addr=0x0000000000001234 val=0x0000000000005678",
        )

    def test_short_binary_payloads_fall_back_safely(self):
        self.assertEqual(
            format_event_value("extlog_event", "cper_data", b"RAS"),
            "52 41 53",
        )
        self.assertEqual(
            format_event_value("nvidia_ns_event", "reg_data", b"RAS"),
            "52 41 53",
        )


if __name__ == "__main__":
    unittest.main()
