#!/usr/bin/env python3
#
# pylint: disable=C0301, C0114
# SPDX-License-Identifier: GPL-2.0
#
# Copyright (C) 2024  Mauro Carvalho Chehab <mchehab@kernel.org>

import argparse
import json
import socket
import sys

einj_description= """
Do UEFI CPER firmware-first error injections via QEMU.

This small utility works together with those QEMU additions:
   https://gitlab.com/mchehab_kernel/qemu/-/tree/arm-error-inject-v2

It allows using UEFI BIOS EINJ features on ARM32/64 emulation to do ARM
processor error injection, to be decoded by Linux CPER and GHES drivers,
helping to test rasdaemon firmware-first error handling for ARM processor
events, being compatible with UEFI 2.9A Errata.
"""


# Valid choice values
ARM_valid_bits = {
    "mpidr": "mpidr-valid",
    "affinity": "affinity-valid",
    "running": "running-state-valid",
    "vendor": "vendor-specific-valid"
}

PEI_flags = {
    "first": "first-error-cap",
    "last": "last-error-cap",
    "propagated": "propagated",
    "overflow": "overflow",
}

PEI_error_types = {
    "cache": "cache-error",
    "tlb": "tlb-error",
    "bus": "bus-error",
    "micro-arch": "micro-arch-error",
    "vendor": "micro-arch-error"
}

PEI_valid_bits = {
    "multiple-error": "multiple-error-valid",
    "flags": "flags-valid",
    "error": "error-info-valid",
    "virt": "virt-addr-valid",
    "virtual": "virt-addr-valid",
    "phy": "phy-addr-valid",
    "physical": "phy-addr-valid"
}

def einj_command(host, port, commands):
    """Send commands to QEMU though QMP TCP socket"""

    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.connect((host, port))

    data = s.recv(1024)

    print("\t", data.decode('utf-8'), end="")

    for command in commands:
        print(command)

        s.sendall(command.encode('utf-8'))
        data = s.recv(1024)
        print("\t", data.decode('utf-8'), end="")

    s.shutdown(socket.SHUT_WR)
    while 1:
        data = s.recv(1024)
        if data == b"":
            break
        print("\t", data.decode('utf-8'))

    s.close()

def get_choice(name, value, choices, suffixes=None):
    new_values = []

    if not value:
        return new_values

    for val in value.split(","):
        val = val.lower()

        if suffixes:
            for suffix in suffixes:
                val = val.removesuffix(suffix)

        if val not in choices.keys():
            sys.exit(f"Error on '{name}': choice {val} is invalid.")

        val = choices[val]

        new_values.append(val)

    return new_values

def get_mult_array(mult, name, values, allow_zero=False, max_val=None):
    if not allow_zero:
        if not values:
            return
    else:
        if values is None:
            return
        elif not values:
            i = 0
            if i not in mult:
                mult[i] = {}

            mult[i][name]=[]
            return

    i = 0
    for value in values:
        for val in value.split(","):
            try:
                val = int(val, 0)
            except:
                sys.exit(f"Error on '{name}': {val} is not an integer")

            if val < 0:
                sys.exit(f"Error on '{name}': {val} is not unsigned")

            if max_val and val > max_val:
                sys.exit(f"Error on '{name}': {val} is too big")

            if i not in mult:
                mult[i] = {}

            if name not in mult[i]:
                mult[i][name] = []

            mult[i][name].append(val)

        i += 1

def get_mult_choices(mult, name, values, choices,
                     suffixes = None, allow_zero=False):
    if not allow_zero:
        if not values:
            return
    else:
        if values is None:
            return

    i = 0
    for val in values:
        new_values = get_choice(name, val, choices, suffixes)

        if i not in mult:
            mult[i] = {}

        mult[i][name] = new_values
        i += 1

def get_mult_int(mult, name, values, allow_zero=False):
    if not allow_zero:
        if not values:
            return
    else:
        if values is None:
            return

    i = 0
    for val in values:
        try:
            val = int(val, 0)
        except:
            sys.exit(f"Error on '{name}': {val} is not an integer")

        if val < 0:
            sys.exit(f"Error on '{name}': {val} is not unsigned")

        if i not in mult:
            mult[i] = {}

        mult[i][name] = val
        i += 1

class errorInjection:
    def __init__(self, args=None, arm=None):

        """Initialize the error injection class. There are two possible
           ways to initialize it:
           1. passing a set of arguments;
           2. passing a dict with error inject command parameters. Each
              column is handled in separate."""

        self.arm = {}

        if not args:
            self.args = args
            return

        print("ARGS:", args)

        pei = {}
        ctx = {}
        vendor = {}

        # Handle global parameters
        if args.arm:
            self.arm["validation"] = get_choice(name="validation", value=args.arm,
                                                choices=ARM_valid_bits,
                                                suffixes=["-error", "-err"])

        if args.affinity:
            self.arm["affinity-level"] = args.affinity

        if args.mpidr:
            self.arm["mpidr-el1"] = args.mpidr

        if args.midr:
            self.arm["midr-el1"] = args.midr

        if args.running is not None:
            if args.running:
                self.arm["running-state"] = ["processor-running"]
            else:
                self.arm["running-state"] = []

        if args.psci:
            self.arm["psci-state"] = args.psci


        # Handle PEI
        if not args.type:
            args.type = ['cache-error']

        get_mult_choices(pei, name="validation", values=args.pei_valid,
                        choices=PEI_valid_bits,
                        suffixes=["-valid", "-info", "--information", "--addr"])
        get_mult_choices(pei, name="type", values=args.type,
                        choices=PEI_error_types,
                        suffixes=["-error", "-err"])
        get_mult_choices(pei, name="flags", values=args.flags,
                        choices=PEI_flags,
                        suffixes=["-error", "-cap"])
        get_mult_int(pei, "multiple-error", args.multiple_error)
        get_mult_int(pei, "phy-addr", args.physical_address)
        get_mult_int(pei, "vir-addr", args.virtual_address)

        # Handle context
        get_mult_int(ctx, "type", args.ctx_type, allow_zero=True)
        get_mult_int(ctx, "minimal-size", args.ctx_size, allow_zero=True)
        get_mult_array(ctx, "register", args.ctx_array, allow_zero=True)

        get_mult_array(vendor, "bytes", args.vendor, max_val=255)

        # Store PEI
        self.arm["error"] = []
        for k in sorted(pei.keys()):
            self.arm["error"].append(pei[k])

        # Store Context
        if ctx:
            self.arm["context"] = []
            for k in sorted(ctx.keys()):
                self.arm["context"].append(ctx[k])

        # Vendor-specific bytes are not grouped
        if vendor:
            self.arm["vendor-specific"] = []
            for k in sorted(vendor.keys()):
                self.arm["vendor-specific"] += vendor[k]["bytes"]

    def run(self, host, port):

        # Fill the commands to be sent
        commands = []

        # Needed to negotiate QMP and for QEMU to accept the command
        commands.append('{ "execute": "qmp_capabilities" } ')

        command = '{ "execute": "arm-inject-error", '
        command += '"arguments": ' + json.dumps(self.arm) + ' }'

        commands.append(command)

        einj_command(host, port, commands)


def handle_args():
    parser = argparse.ArgumentParser(prog="einj.py",
                                     usage="%(prog)s [options]",
                                     description=einj_description,
                                     epilog="If a field is not defined, a default value will be applied by QEMU.")

    # TCP QEMU QMP host/port
    g_options = parser.add_argument_group("Socket options")
    g_options.add_argument("-H", "--host", default="localhost", type=str,
                           help="host name")
    g_options.add_argument("-P", "--port", default=4445, type=int,
                           help="TCP port number")

    # UEFI N.16 ARM Validation bits
    g_arm = parser.add_argument_group("ARM processor")
    g_arm.add_argument("--arm", "--arm-valid",
                       help="ARM validation bits: mpidr,affinity,running,vendor")
    g_arm.add_argument("-a", "--affinity",  "--level", "--affinity-level", type=lambda x: int(x, 0),
                       help="Affinity level (when multiple levels apply)")
    g_arm.add_argument("-l", "--mpidr", type=lambda x: int(x, 0),
                       help="Multiprocessor Affinity Register")
    g_arm.add_argument("-i", "--midr", type=lambda x: int(x, 0),
                       help="Main ID Register")
    g_arm.add_argument("-r", "--running",
                       action=argparse.BooleanOptionalAction, default=None,
                       help="Indicates if the processor is running or not")
    g_arm.add_argument("--psci", "--psci-state", type=lambda x: int(x, 0),
                       help="Power State Coordination Interface - PSCI state")

    # TODO: Add vendor-specific support

    # UEFI N.17 bitmaps (type and flags)
    g_pei = parser.add_argument_group("ARM Processor Error Info (PEI)")
    g_pei.add_argument("-t", "--type", nargs="+",
                       help="one or more error types: cache,tlb,bus,vendor")
    g_pei.add_argument("-f", "--flags", nargs="*",
                       help="zero or more error flags: first-error,last-error,propagated,overflow")
    g_arm.add_argument("-V", "--pei-valid", "--error-valid", nargs="*",
                       help="zero or more PEI validation bits: multiple-error,flags,error-info,virt,phy")

    # UEFI N.17 Integer values
    g_pei.add_argument("-m", "--multiple-error", nargs="+",
                       help="Number of errors: 0: Single error, 1: Multiple errors, 2-65535: Error count if known")
    g_pei.add_argument("-e", "--error-info", nargs="+",
                       help="Error information (UEFI 2.10 tables N.18 to N.20)")
    g_pei.add_argument("-p", "--physical-address",  nargs="+",
                       help="Physical address")
    g_pei.add_argument("-v", "--virtual-address",  nargs="+",
                       help="Virtual address")

    # UEFI N.21 Context
    g_pei.add_argument("--ctx-type", "--context-type", nargs="*",
                       help="Type of the context (0=ARM32 GPR, 5=ARM64 EL1, other values supported)")
    g_pei.add_argument("--ctx-size", "--context-size", nargs="*",
                       help="Minimal size of the context")
    g_pei.add_argument("--ctx-array", "--context-array", nargs="*",
                       help="Comma-separated arrays for each context")

    # Vendor-specific data
    g_pei.add_argument("--vendor", "--vendor-specific", nargs="+",
                       help="Vendor-specific byte arrays of data")

    return parser.parse_args()

def main():
    """Main program"""

    args = handle_args()

    einj = errorInjection(args)
    einj.run(args.host, args.port)

if __name__ == '__main__':
    main()
