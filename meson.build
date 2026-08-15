# SPDX-License-Identifier: GPL-2.0
#
# Copyright (C) 2026 Mauro Carvalho Chehab <mchehab+huawei@kernel.org>

project('RASdaemon', 'c',
        version: '0.9.90',
        license: 'GPL-2.0',
        meson_version: '>= 0.58')

#
# Compiler
#
cc = meson.get_compiler('c')

fs = import('fs')

c_flags = [
    '-D_GNU_SOURCE',
    '-D RASSTATEDIR="' + get_option('statedir') + '"',
    '-DRAS_DB_FNAME="' + get_option('db-fname') + '"',
    '-DVERSION="' + meson.project_version() + '"',
]

unittest_sources = [
    'tests/main.c',
    'tests/test-modules.c',
]

#
# Dependencies
#
mysql_dep = dependency('mysqlclient', required: false)
pg_dep = dependency('libpq', required: false)
sqlite3_dep = dependency('sqlite3', required: false)

if mysql_dep.found() and not get_option('mysql').disabled()
    enable_mysql = true
else
    enable_mysql = false
    message('MySQL client devel not found: disabling MySQL support')
endif

if sqlite3_dep.found() and not get_option('sqlite3').disabled()
    enable_sqlite3 = true
else
    enable_sqlite3 = false
    message('SQLite3 devel not found: disabling SQLite3 support')
endif

libtraceevent_dep = dependency('libtraceevent', required: true)
libpci_dep = dependency('libpci', required: not get_option('aer').disabled())
cmocka_dep = dependency('cmocka', required: false)

dependencies = [
    mysql_dep,
    pg_dep,
    sqlite3_dep,

    libtraceevent_dep,
    libpci_dep
]


#
# Library sources
#
lib_sources = [
    'core/bitfield.c',
    'core/modules.c',
    'core/ras-logger.c',
    'core/ras-events.c',
    'core/ras-mc-handler.c',
    'core/trigger.c',
    'core/types.c',
]

#
# MCE-specific sources
#
mce_sources = [
    'events-arch-x86/mce-amd.c',
    'events-arch-x86/mce-amd-k8.c',
    'events-arch-x86/mce-amd-smca.c',
    'events-arch-x86/mce-intel.c',
    'events-arch-x86/mce-intel-broadwell-de.c',
    'events-arch-x86/mce-intel-broadwell-epex.c',
    'events-arch-x86/mce-intel-diamond.c',
    'events-arch-x86/mce-intel-dunnington.c',
    'events-arch-x86/mce-intel-haswell.c',
    'events-arch-x86/mce-intel-granite.c',
    'events-arch-x86/mce-intel-i10nm.c',
    'events-arch-x86/mce-intel-ivb.c',
    'events-arch-x86/mce-intel-knl.c',
    'events-arch-x86/mce-intel-nehalem.c',
    'events-arch-x86/mce-intel-p4-p6.c',
    'events-arch-x86/mce-intel-sb.c',
    'events-arch-x86/mce-intel-skylake-xeon.c',
    'events-arch-x86/mce-intel-tulsa.c',
    'events-arch-x86/mce-zhaoxin.c',
    'events-arch-x86/mce-zhaoxin-kh50000.c',
    'events-arch-x86/ras-mce-handler.c',
]

if not get_option('abrt-report').disabled()
    lib_sources += 'modules/ras-report.c'
endif

if not get_option('aer').disabled()
    lib_sources += 'modules/ras-aer-handler.c'
endif

if not get_option('amp-ns-decode').disabled()
    lib_sources += 'events-arch-arm/non-standard-ampere.c'
endif

if not get_option('arm').disabled()
    lib_sources += 'events-arch-arm/ras-arm-handler.c'
endif

if not get_option('cpu-fault-isolation').disabled()
    lib_sources += [
        'modules/ras-cpu-isolation.c',
        'core/queue.c'
    ]
endif

if not get_option('cxl').disabled()
    lib_sources += 'events/ras-cxl-handler.c'
endif

if not get_option('devlink').disabled()
    lib_sources += 'events/ras-devlink-handler.c'
endif

if not get_option('diskerror').disabled()
    lib_sources += 'events/ras-diskerror-handler.c'
endif

if not get_option('erst').disabled()
    lib_sources += 'events-arch-x86/ras-erst.c'
endif

if not get_option('extlog').disabled()
    lib_sources += 'core/ras-extlog-handler.c'
endif

if not get_option('hisi-ns-decode').disabled()
    lib_sources += [
        'events-arch-arm/non-standard-hisi_hip08.c',
        'events-arch-arm/non-standard-hisilicon.c',
    ]
endif

if not get_option('jaguar-ns-decode').disabled()
    lib_sources += 'events-arch-arm/non-standard-jaguarmicro.c'
endif

if not get_option('mce').disabled()
    lib_sources += mce_sources
endif

if not get_option('memory-failure').disabled()
    lib_sources += 'events/ras-memory-failure-handler.c'
endif

if not get_option('nvidia-ns-decode').disabled()
    lib_sources += 'events-arch-arm/non-standard-nvidia.c'
endif

if not get_option('non-standard').disabled()
    lib_sources += 'events-arch-arm/ras-non-standard-handler.c'
endif

if not get_option('openbmc-unified-sel').disabled()
    lib_sources += 'events/unified-sel.c'
endif

if not get_option('memory-ce-pfa').disabled() or \
   not get_option('memory-row-ce-pfa').disabled()

    lib_sources += [
        'modules/ras-page-isolation.c',
        'core/rbtree.c',
    ]
endif

if not get_option('memory-ce-pfa').disabled() or \
   not get_option('memory-row-ce-pfa').disabled() or \
   not get_option('memory-failure').disabled()
    lib_sources += 'events/ras-poison-page-stat.c'
endif


if not get_option('signal').disabled()
    lib_sources += 'events/ras-signal-handler.c'
endif

if enable_sqlite3 or enable_mysql
    lib_sources += [
        'db/ras-db.c',
        'db/ras-record.c',
    ]
endif

if enable_mysql
    lib_sources += 'db/db-mysql.c'
    unittest_sources += 'tests/test-mysql.c'
endif

if enable_sqlite3
    lib_sources += ['db/db-sqlite3.c']
    unittest_sources += 'tests/test-sqlite3.c'
endif

if not get_option('yitian-ns-decode').disabled()
    lib_sources += 'events-arch-arm/non-standard-yitian.c'
endif

if not get_option('reri').disabled()
    lib_sources += 'events/ras-reri-handler.c'
endif

if not get_option('cpu-fault-isolation').disabled()
    lib_sources += [
        'core/queue.c',
        'modules/ras-cpu-isolation.c',
    ]
endif

#
# Main program sources
#
rasdaemon_sources = ['core/rasdaemon.c']

#
# Headers
#
headers = [
    'core/bitfield.h',
    'core/modules.h',
    'core/queue.h',
    'core/ras-events.h',
    'core/ras-extlog-handler.h',
    'core/ras-logger.h',
    'core/ras-mc-handler.h',
    'core/rbtree.h',
    'core/trigger.h',
    'core/types.h',
    'db/db-mysql.h',
    'db/db-sqlite3.h',
    'db/ras-db-backend.h',
    'db/ras-db.h',
    'db/ras-record.h',
    'events-arch-arm/non-standard-ampere.h',
    'events-arch-arm/non-standard-hisilicon.h',
    'events-arch-arm/non-standard-jaguarmicro.h',
    'events-arch-arm/non-standard-nvidia.h',
    'events-arch-arm/non-standard-yitian.h',
    'events-arch-arm/ras-arm-handler.h',
    'events-arch-arm/ras-non-standard-handler.h',
    'events-arch-x86/ras-erst.h',
    'events-arch-x86/ras-mce-handler.h',
    'events/ras-cxl-handler.h',
    'events/ras-devlink-handler.h',
    'events/ras-diskerror-handler.h',
    'events/ras-memory-failure-handler.h',
    'events/ras-poison-page-stat.h',
    'events/ras-reri-handler.h',
    'events/ras-signal-handler.h',
    'events/unified-sel.h',
    'modules/ras-aer-handler.h',
    'modules/ras-cpu-isolation.h',
    'modules/ras-page-isolation.h',
    'modules/ras-report.h',
    'tests/unittest.h',
]

#
# Static library
#
librasdaemon = static_library(
    'rasdaemon',
    lib_sources,
    dependencies: dependencies,
    c_args: c_flags,
)

#
# Main executable
#
rasdaemon_exe = executable(
    'rasdaemon',
    rasdaemon_sources,
    dependencies: dependencies,
    link_with: [librasdaemon],
    c_args: c_flags,
)

#
# Unit tests
#
unittest_deps = dependencies

if cmocka_dep.found()
    unittest_deps += cmocka_dep
endif

unittest_exe = executable(
    'unittest', unittest_sources,
    dependencies: dependencies + unittest_deps,
    link_whole: [librasdaemon],
    c_args: ['-g'] + c_flags,
)

#
# Extra install files
#

install_map = {
    'Makefile.inc.in': '',
    'misc/rasdaemon.spec.in': '',

    # FIXME: add/adjust install paths for those
    'man/rasdaemon.1.in': 'man/man1',
    'man/ras-mc-ctl.8.in': 'man/man1',
    'misc/rasdaemon.logrotate.in': '',
    'misc/rasdaemon.rsyslog.in': '',
    'misc/rasdaemon.service.in': '',
    'misc/rasdaemon.syslog-ng.in': '',
    'misc/ras-mc-ctl.service.in': '',
    'util/ras-mc-ctl.in': '',
}

foreach in_file, install_dir : install_map
    base = fs.name(in_file)
    out_file = base.strip('.in')

    message(out_file)

    # Generate the file (output goes to build dir)
    configured = configure_file(
        input: in_file,
        output: out_file,
        configuration : {
            'PACKAGE' :  meson.project_name(),
            'PACKAGE_VERSION' :  meson.project_version(),
        }
    )

    if install_dir != ''
        install_data(configured, install_dir: install_dir)
    endif
endforeach

#
# Auto-generated config.h file
#

configure_file(
    input : 'config.h.in',
    output : 'config.h',
    configuration : {
        # Keep in alphabetic order
        'HAVE_ABRT_REPORT' : not get_option('abrt-report').disabled() ? 1 : 0,
        'HAVE_AER' : not get_option('aer').disabled() ? 1 : 0,
        'HAVE_AMP_NS_DECODE' : not get_option('amp-ns-decode').disabled() ? 1 : 0,
        'HAVE_ARM' : not get_option('arm').disabled() ? 1 : 0,
        'HAVE_CPU_FAULT_ISOLATION' : not get_option('cpu-fault-isolation').disabled() ? 1 : 0,
        'HAVE_CXL' : not get_option('cxl').disabled() ? 1 : 0,
        'HAVE_DEVLINK' : not get_option('devlink').disabled() ? 1 : 0,
        'HAVE_DISKERROR' : not get_option('diskerror').disabled() ? 1 : 0,
        'HAVE_ERST' : not get_option('erst').disabled() ? 1 : 0,
        'HAVE_EXTLOG' : not get_option('extlog').disabled() ? 1 : 0,
        'HAVE_HISI_NS_DECODE' : not get_option('hisi-ns-decode').disabled() ? 1 : 0,
        'HAVE_JAGUAR_NS_DECODE' : not get_option('jaguar-ns-decode').disabled() ? 1 : 0,
        'HAVE_MCE' : not get_option('mce').disabled() ? 1 : 0,
        'HAVE_MEMORY_CE_PFA' : not get_option('memory-ce-pfa').disabled() ? 1 : 0,
        'HAVE_MEMORY_FAILURE' : not get_option('memory-failure').disabled() ? 1 : 0,
        'HAVE_MEMORY_ROW_CE_PFA' : not get_option('memory-row-ce-pfa').disabled() ? 1 : 0,
        'HAVE_MYSQL' : enable_mysql ? 1 : 0,
        'HAVE_NON_STANDARD' : not get_option('non-standard').disabled() ? 1 : 0,
        'HAVE_NVIDIA_NS_DECODE' : not get_option('nvidia-ns-decode').disabled() ? 1 : 0,
        'HAVE_OPENBMC_UNIFIED_SEL' : not get_option('openbmc-unified-sel').disabled() ? 1 : 0,
        'HAVE_RERI' : not get_option('reri').disabled() ? 1 : 0,
        'HAVE_SIGNAL' : not get_option('signal').disabled() ? 1 : 0,
        'HAVE_SQLITE3' : enable_sqlite3 ? 1 : 0,
        'HAVE_YITIAN_NS_DECODE' : not get_option('yitian-ns-decode').disabled() ? 1 : 0,
    }
)

#
# Summary of enabled features
#

summary(
    {
        # Keep in alphabetic order
        'ABRT report' : not get_option('abrt-report').disabled(),
        'AER' : not get_option('aer').disabled(),
        'CPU fault isolation' : not get_option('cpu-fault-isolation').disabled(),
        'CXL events' : not get_option('cxl').disabled(),
        'DEVLINK' : not get_option('devlink').disabled(),
        'Disk I/O errors' : not get_option('diskerror').disabled(),
        'ERST' : not get_option('erst').disabled(),
        'EXTLOG' : not get_option('extlog').disabled(),
        'Memory CE PFA' : not get_option('memory-ce-pfa').disabled(),
        'Memory Failure' : not get_option('memory-failure').disabled(),
        'Memory ROW CE PFA' : not get_option('memory-row-ce-pfa').disabled(),
        'OpenBMC unified' : not get_option('openbmc-unified-sel').disabled(),
        'Signal' : not get_option('signal').disabled(),
    },
    section : 'General options',
)

summary(
    {
        'MySQL/MariaDB' : enable_mysql,
        'Sqlite3' : enable_sqlite3,
    },
    section : 'Database backends',
)

summary(
    {
        # Keep in alphabetic order
        'ARM events' : not get_option('arm').disabled(),
        'MCE' : not get_option('mce').disabled(),
        'RERI' : not get_option('reri').disabled(),

    },
    section : 'Arch-specific options',
)

summary(
    {
        'CPER non-standard' : not get_option('non-standard').disabled(),

        # Keep in alphabetic order
        'AMP RAS errors' : not get_option('amp-ns-decode').disabled(),
        'HISI Kunpeng errors' : not get_option('hisi-ns-decode').disabled(),
        'YITIAN RAS errors' : not get_option('yitian-ns-decode').disabled(),
        'JAGUAR RAS errors' : not get_option('jaguar-ns-decode').disabled(),
        'NVIDIA RAS errors' : not get_option('nvidia-ns-decode').disabled(),
    },
    section : 'Non-standard CPER records',
)

summary(
    {
        'State directory': get_option('statedir'),
        'SQLite3 database': get_option('db-fname'),

        'rasdaemon version' : meson.project_version(),
    },
    section : 'Arguments and version',
)
