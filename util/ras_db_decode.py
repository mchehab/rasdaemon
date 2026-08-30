# SPDX-License-Identifier: GPL-2.0-only
# Copyright (C) 2026 Mauro Carvalho Chehab <mchehab+huawei@kernel.org>

"""Human-readable formatting for values stored by rasdaemon handlers."""

import struct
import uuid
from typing import Any, Mapping, Sequence


EXTLOG_TYPES = (
    "unknown",
    "no error",
    "single-bit ECC",
    "multi-bit ECC",
    "single-symbol chipkill ECC",
    "multi-symbol chipkill ECC",
    "master abort",
    "target abort",
    "parity error",
    "watchdog timeout",
    "invalid address",
    "mirror Broken",
    "memory sparing",
    "scrub corrected error",
    "scrub uncorrected error",
    "physical memory map-out event",
)
EXTLOG_SEVERITIES = (
    "recoverable", "fatal", "corrected", "informational",
)

CXL_AER_UE = {
    0x00001: "Cache Data Parity Error",
    0x00002: "Cache Address Parity Error",
    0x00004: "Cache Byte Enable Parity Error",
    0x00008: "Cache Data ECC Error",
    0x00010: "Memory Data Parity Error",
    0x00020: "Memory Address Parity Error",
    0x00040: "Memory Byte Enable Parity Error",
    0x00080: "Memory Data ECC Error",
    0x00100: "REINIT Threshold Hit",
    0x00200: "Received Unrecognized Encoding",
    0x00400: "Received Poison From Peer",
    0x00800: "Receiver Overflow",
    0x04000: "Component Specific Error",
    0x08000: "IDE Tx Error",
    0x10000: "IDE Rx Error",
}
CXL_AER_CE = {
    0x01: "Cache Data ECC Error",
    0x02: "Memory Data ECC Error",
    0x04: "CRC Threshold Hit",
    0x08: "Retry Threshold",
    0x10: "Received Cache Poison From Peer",
    0x20: "Received Memory Poison From Peer",
    0x40: "Received Error From Physical Layer",
}
CXL_HEADER_FLAGS = {
    1 << 2: "PERMANENT_CONDITION",
    1 << 3: "MAINTENANCE_NEEDED",
    1 << 4: "PERFORMANCE_DEGRADED",
    1 << 5: "HARDWARE_REPLACEMENT_NEEDED",
    1 << 6: "MAINT_OP_SUB_CLASS_VALID",
    1 << 7: "LOGICAL_DEV_ID_VALID",
    1 << 8: "DEV_HEAD_ID_VALID",
}
CXL_DPA_FLAGS = {
    1 << 0: "VOLATILE",
    1 << 1: "NOT_REPAIRABLE",
}
CXL_DESCRIPTOR_FLAGS = {
    1 << 0: "UNCORRECTABLE EVENT",
    1 << 1: "THRESHOLD EVENT",
    1 << 2: "POISON LIST OVERFLOW",
}
CXL_CME_FLAGS = {
    1 << 0: "Corrected Memory Errors in Multiple Media Components",
    1 << 1: "Exceeded Programmable Threshold",
}
CXL_HEALTH_FLAGS = {
    1 << 0: "MAINTENANCE_NEEDED",
    1 << 1: "PERFORMANCE_DEGRADED",
    1 << 2: "REPLACEMENT_NEEDED",
    1 << 3: "MEM_CAPACITY_DEGRADED",
}
CXL_SPARING_FLAGS = {
    1 << 0: "QUERY_RESOURCES",
    1 << 1: "HARD_SPARING",
    1 << 2: "DEVICE_INITIATED",
}

CXL_MEMORY_SUB_TYPES = (
    "Not Reported",
    "Internal Datapath Error",
    "Media Link Command Training Error",
    "Media Link Control Training Error",
    "Media Link Data Training Error",
    "Media Link CRC Error",
)
CXL_GENERAL_MEDIA_TYPES = (
    "ECC Error",
    "Invalid Address",
    "Data Path Error",
    "TE State Violation",
    "Scrub Media ECC Error",
    "Advanced Programmable CME Counter Expiration",
    "CKID Violation",
)
CXL_DRAM_TYPES = (
    "Media ECC Error",
    "Scrub Media ECC Error",
    "Invalid Address",
    "Data Path Error",
    "TE State Violation",
    "Advanced Programmable CME Counter Expiration",
    "CKID Violation",
)
CXL_TRANSACTIONS = (
    "Unknown",
    "Host Read",
    "Host Write",
    "Host Scan Media",
    "Host Inject Poison",
    "Internal Media Scrub",
    "Internal Media Management",
    "Internal Media Error Check Scrub",
    "Media Initialization",
)
CXL_MODULE_EVENT_TYPES = (
    "Health Status Change",
    "Media Status Change",
    "Life Used Change",
    "Temperature Change",
    "Data Path Error",
    "LSA Error",
    "Unrecoverable Internal Sideband Bus Error",
    "Memory Media FRU Error",
    "Power Management Fault",
)
CXL_MODULE_EVENT_SUB_TYPES = (
    "Not Reported",
    "Invalid Config Data",
    "Unsupported Config Data",
    "Unsupported Memory Media FRU",
)
CXL_MEDIA_STATUS = (
    "Normal",
    "Not Ready",
    "Write Persistency Lost",
    "All Data Lost",
    "Write Persistency Loss in the Event of Power Loss",
    "Write Persistency Loss in Event of Shutdown",
    "Write Persistency Loss Imminent",
    "All Data Loss in Event of Power Loss",
    "All Data loss in the Event of Shutdown",
    "All Data Loss Imminent",
)

_CPER_MEMORY = struct.Struct("<Q8H3Q3H")
_CPER_FIELDS = (
    (0x0008, "node", 1, False),
    (0x0010, "card", 2, False),
    (0x0020, "module", 3, False),
    (0x0040, "bank", 4, False),
    (0x0080, "device", 5, False),
    (0x0100, "row", 6, False),
    (0x0200, "column", 7, False),
    (0x0400, "bit_position", 8, False),
    (0x0800, "requestor_id", 9, True),
    (0x1000, "responder_id", 10, True),
    (0x2000, "target_id", 11, True),
    (0x8000, "rank", 12, False),
    (0x10000, "mem_array_handle", 13, False),
    (0x20000, "mem_dev_handle", 14, False),
)

CXL_TABLES = {
    "cxl_aer_ue_event", "cxl_aer_ce_event", "cxl_overflow_event",
    "cxl_poison_event", "cxl_generic_event",
    "cxl_general_media_event", "cxl_dram_event",
    "cxl_memory_module_event", "cxl_memory_sparing_event",
}
CXL_HEX_FIELDS = {
    "serial", "hdr_handle", "hdr_related_handle", "hdr_ld_id",
    "hdr_head_id", "dpa", "hpa", "hpa_alias0", "dpa_length",
}
CXL_HEX_BLOBS = {
    "data", "comp_id", "pldm_entity_id", "pldm_resource_id", "cor_mask",
}


def _as_bytes(value: Any) -> bytes | None:
    if isinstance(value, (bytes, bytearray, memoryview)):
        return bytes(value)
    return None


def _enum(value: Any, names: Sequence[str], unknown: str = "unknown") -> str:
    if not isinstance(value, int):
        return str(value)
    name = names[value] if 0 <= value < len(names) else unknown
    return f"{value} ({name})"


def _flags(value: Any, names: Mapping[int, str]) -> str:
    if not isinstance(value, int):
        return str(value)
    decoded = [name for bit, name in names.items() if value & bit]
    known = 0
    for bit in names:
        known |= bit
    unknown = value & ~known
    if unknown:
        decoded.append(f"unknown=0x{unknown:x}")
    suffix = f" ({' | '.join(decoded)})" if decoded else ""
    return f"0x{value:x}{suffix}"


def _hex_blob(value: Any) -> str:
    data = _as_bytes(value)
    if data is None:
        return str(value)
    return data.hex(" ")


def _uuid_le(value: Any) -> str:
    data = _as_bytes(value)
    if data is None or len(data) != 16:
        return _hex_blob(value)
    return str(uuid.UUID(bytes_le=data))


def _cper_memory(value: Any) -> str:
    data = _as_bytes(value)
    if data is None or len(data) < _CPER_MEMORY.size:
        return _hex_blob(value)
    fields = _CPER_MEMORY.unpack_from(data)
    validation = fields[0]
    decoded = []
    for bit, name, index, hexadecimal in _CPER_FIELDS:
        if validation & bit:
            item = fields[index]
            decoded.append(
                f"{name}=0x{item:x}" if hexadecimal else f"{name}={item}"
            )
    return ", ".join(decoded) if decoded else _hex_blob(value)


def _header_log(value: Any) -> str:
    data = _as_bytes(value)
    if data is None:
        return str(value)
    complete = len(data) - len(data) % 4
    words = [
        f"{word[0]:08x}" for word in struct.iter_unpack(">I", data[:complete])
    ]
    if complete != len(data):
        words.append(f"trailing={data[complete:].hex()}")
    return " ".join(words)


def _nvidia_registers(value: Any) -> str:
    data = _as_bytes(value)
    if data is None:
        return str(value)
    complete = len(data) - len(data) % 16
    if not complete:
        return _hex_blob(value)
    registers = [
        f"Reg[{index}]: addr=0x{address:016x} val=0x{register:016x}"
        for index, (address, register) in enumerate(
            struct.iter_unpack("<QQ", data[:complete])
        )
    ]
    if complete != len(data):
        registers.append(f"trailing={data[complete:].hex()}")
    return "; ".join(registers) if registers else _hex_blob(value)


def _additional_status(value: Any) -> str:
    if not isinstance(value, int):
        return str(value)
    two_bit = ("Normal", "Warning", "Critical")
    one_bit = ("Normal", "Warning")

    def status(names: Sequence[str], index: int) -> str:
        return names[index] if index < len(names) else "unknown"

    decoded = (
        f"life_used={status(two_bit, value & 0x3)}",
        f"device_temperature={status(two_bit, (value >> 2) & 0x3)}",
        f"corrected_volatile={status(one_bit, (value >> 4) & 0x1)}",
        f"corrected_persistent={status(one_bit, (value >> 5) & 0x1)}",
    )
    return f"0x{value:x} ({', '.join(decoded)})"


def _format_cxl(table: str, field: str, value: Any) -> str | None:
    if table not in CXL_TABLES:
        return None
    if field in CXL_HEX_FIELDS and isinstance(value, int):
        return f"0x{value:x}"
    if field == "hdr_flags":
        return _flags(value, CXL_HEADER_FLAGS)
    if table == "cxl_aer_ue_event" and field in (
            "error_status", "first_error"):
        return _flags(value, CXL_AER_UE)
    if table == "cxl_aer_ce_event" and field == "error_status":
        return _flags(value, CXL_AER_CE)
    if field == "header_log":
        return _header_log(value)
    if field in CXL_HEX_BLOBS:
        return _hex_blob(value)
    if field == "dpa_flags":
        return _flags(value, CXL_DPA_FLAGS)
    if field == "descriptor":
        return _flags(value, CXL_DESCRIPTOR_FLAGS)
    if field == "sub_type" and table in (
            "cxl_general_media_event", "cxl_dram_event"):
        return _enum(value, CXL_MEMORY_SUB_TYPES, "unknown-type")
    if field == "transaction_type" and table in (
            "cxl_general_media_event", "cxl_dram_event"):
        return _enum(value, CXL_TRANSACTIONS, "unknown-type")
    if field == "type" and table == "cxl_general_media_event":
        return _enum(value, CXL_GENERAL_MEDIA_TYPES, "unknown-type")
    if field == "type" and table == "cxl_dram_event":
        return _enum(value, CXL_DRAM_TYPES, "unknown-type")
    if field == "cme_threshold_ev_flags":
        return _flags(value, CXL_CME_FLAGS)
    if table == "cxl_memory_module_event":
        if field == "event_type":
            return _enum(value, CXL_MODULE_EVENT_TYPES, "unknown-type")
        if field == "event_sub_type":
            return _enum(value, CXL_MODULE_EVENT_SUB_TYPES, "unknown-type")
        if field == "health_status":
            return _flags(value, CXL_HEALTH_FLAGS)
        if field == "media_status":
            return _enum(value, CXL_MEDIA_STATUS)
        if field == "add_status":
            return _additional_status(value)
    if table == "cxl_memory_sparing_event":
        if field == "flags":
            return _flags(value, CXL_SPARING_FLAGS)
        if field == "result" and isinstance(value, int):
            return f"0x{value:x}"
    return None


def format_event_value(table: str, field: str, value: Any) -> str:
    """Return a human-readable value while retaining its stored identity."""

    if table == "extlog_event":
        if field == "etype":
            return _enum(value, EXTLOG_TYPES, "unknown-type")
        if field == "severity":
            return _enum(value, EXTLOG_SEVERITIES, "unknown-severity")
        if field == "address" and isinstance(value, int):
            return f"0x{value:x}"
        if field == "fru_id":
            return _uuid_le(value)
        if field == "cper_data":
            return _cper_memory(value)

    cxl = _format_cxl(table, field, value)
    if cxl is not None:
        return cxl

    if table == "nvidia_ns_event":
        if field == "instance_base" and isinstance(value, int):
            return f"0x{value:x}"
        if field == "reg_data":
            return _nvidia_registers(value)
    if table == "nvidia_vera_ns_event":
        if field in ("event_link_id", "architecture", "instance_base") \
                and isinstance(value, int):
            return f"0x{value:x}"
        if field == "chip_serial_number":
            data = _as_bytes(value)
            return data.hex() if data is not None else str(value)

    if table == "arm_event" and field in (
            "mpidr", "running_state", "psci_state") \
            and isinstance(value, int):
        return f"0x{value:x}"

    return str(value)
