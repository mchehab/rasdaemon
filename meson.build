# SPDX-License-Identifier: GPL-2.0
#
# Copyright (C) 2026 Mauro Carvalho Chehab <mchehab+huawei@kernel.org>

project('rasdaemon', 'c',
        version: '0.9.90',
        license: 'GPL-2.0',
        meson_version: '>= 0.60')

#
# Compiler
#
cc = meson.get_compiler('c')

fs = import('fs')

c_flags = [
    '-D_GNU_SOURCE',
    '-DSQLITE3_DATABASE="' + get_option('sqlite3-database') + '"',
    '-DVERSION="' + meson.project_version() + '"',
]

unittest_sources = [
    'tests/main.c',
    'tests/trace-mock.c',
    'tests/test-core.c',
    'tests/test-events.c',
    'tests/test-modules.c',
]

#
# Extra prefix directories
#

prefix  = get_option('prefix')
libdir  = get_option('libdir')
datadir = get_option('datadir')

py = import('python').find_installation('python3')

arch = get_option('enable-arch')
if arch == 'auto'
  host_arch = host_machine.cpu_family()
  if host_arch in ['x86', 'x86_64']
    arch = 'x86'
  elif host_arch in ['arm', 'aarch64']
    arch = 'arm'
  elif host_arch in ['riscv32', 'riscv64']
    arch = 'riscv'
  endif
endif
arch_x86 = arch == 'all' or arch == 'x86'
arch_arm = arch == 'all' or arch == 'arm'
arch_riscv = arch == 'all' or arch == 'riscv'

pydir = py.get_install_dir(pure: true)

#
# Global dependencies and install directory paths
#

libtraceevent_dep = dependency('libtraceevent', required: true)

systemd_dep = dependency('systemd', required: false)
if systemd_dep.found()
  systemd_unitdir = systemd_dep.get_variable(
    pkgconfig: 'systemdsystemunitdir',
    default_value: join_paths(libdir, 'systemd', 'system')
  )
else
  systemd_unitdir = join_paths(libdir, 'systemd', 'system')
endif

if prefix != '/usr'
  if systemd_unitdir.startswith('/')
    systemd_unitdir = prefix + systemd_unitdir
  else
    systemd_unitdir = join_paths(prefix, systemd_unitdir)
  endif
endif

bash_compdir = prefix / datadir / 'bash-completion' / 'completions'
zsh_compdir = prefix / datadir  / 'zsh' / 'site-functions'

dependencies = [
    libtraceevent_dep,
]

#
# Library sources
#
lib_sources = [
    'core/bitfield.c',
    'core/modules.c',
    'core/ras-logger.c',
    'core/ras-env.c',
    'core/ras-events.c',
    'events/ras-mc-handler.c',
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

# Enable variables for each feature. Keep it in alphabetic order
enable_abrt_report = false
enable_aer = false
enable_amp_ns_decode = false
enable_arm = false
enable_cpu_fault_isolation = false
enable_cxl = false
enable_db = false
enable_devlink = false
enable_diskerror = false
enable_erst = false
enable_extlog = false
enable_hisi_ns_decode = false
enable_jaguar_ns_decode = false
enable_mce = false
enable_memory_ce_pfa = false
enable_memory_failure = false
enable_memory_row_ce_pfa = false
enable_mysql = false
enable_non_standard = false
enable_nvidia_ns_decode = false
enable_openbmc_unified_sel = false
enable_page_isolation = false
enable_poison_page_stat = false
enable_postgresql = false
enable_reri = false
enable_signal = false
enable_sqlite3 = false
enable_yitian_ns_decode = false

if not get_option('disable-all')
    #
    # Features that require dependencies
    #
    mysql_dep = dependency('mysqlclient', 'mariadb', 'libmariadb', required: false)

    if mysql_dep.found() and not get_option('mysql').disabled()
        if cc.has_header('mysql/mysql.h')
            enable_db = true
            enable_mysql = true
            dependencies += mysql_dep
            lib_sources += 'db/db-mysql.c'
            unittest_sources += 'tests/test-mysql.c'
        else
            name = mysql_dep.name()
            ver = mysql_dep.version()
            message(name + ' version ' + ver + ': mysql.h not found!')
        endif
    else
        message('MySQL client devel not found: disabling MySQL support')
    endif

    pq_dep = dependency('libpq', required: false)
    if pq_dep.found() and not get_option('postgresql').disabled()
        enable_db = true
        enable_postgresql = true
        lib_sources += ['db/db-postgresql.c']
        unittest_sources += 'tests/test-postgresql.c'
        dependencies += pq_dep
    else
        message('libpq devel not found: disabling PostgreSQL support')
    endif

    sqlite3_dep = dependency('sqlite3', required: false)
    if sqlite3_dep.found() and not get_option('sqlite3').disabled()
        enable_db = true
        enable_sqlite3 = true
        lib_sources += ['db/db-sqlite3.c']
        unittest_sources += 'tests/test-sqlite3.c'
        dependencies += sqlite3_dep
    else
        message('SQLite3 devel not found: disabling SQLite3 support')
    endif

    libpci_dep = dependency('libpci', required: not get_option('aer').disabled())
    if not get_option('aer').disabled()
        if libpci_dep.found()
            enable_aer = true
            lib_sources += 'events/ras-aer-handler.c'
            dependencies += libpci_dep
        else
          message('libpci not found: disabling AER support')
        endif
    endif

    #
    # Features that can be built without any external deps
    #

    if not get_option('abrt-report').disabled()
        enable_abrt_report = true
        lib_sources += 'modules/ras-report.c'
    endif

    if arch_arm and not get_option('amp-ns-decode').disabled()
        enable_amp_ns_decode = true
        enable_non_standard = true
        lib_sources += 'events-arch-arm/non-standard-ampere.c'
    endif

    if arch_arm and not get_option('arm').disabled()
        enable_arm = true
        lib_sources += 'events-arch-arm/ras-arm-handler.c'
    endif

    if not get_option('cpu-fault-isolation').disabled()
        enable_cpu_fault_isolation = true
        lib_sources += [
            'modules/ras-cpu-isolation.c',
            'core/queue.c'
        ]
    endif

    if not get_option('cxl').disabled()
        enable_cxl = true
        lib_sources += 'events/ras-cxl-handler.c'
    endif

    if not get_option('devlink').disabled()
        enable_devlink = true
        lib_sources += 'events/ras-devlink-handler.c'
    endif

    if not get_option('diskerror').disabled()
        enable_diskerror = true
        lib_sources += 'events/ras-diskerror-handler.c'
    endif

    if arch_x86 and not get_option('erst').disabled()
        enable_erst = true
        lib_sources += 'events-arch-x86/ras-erst.c'
    endif

    if not get_option('extlog').disabled()
        enable_extlog = true
        lib_sources += 'events/ras-extlog-handler.c'
    endif

    if arch_arm and not get_option('hisi-ns-decode').disabled()
        enable_hisi_ns_decode = true
        enable_non_standard = true
        lib_sources += [
            'events-arch-arm/non-standard-hisi_hip08.c',
            'events-arch-arm/non-standard-hisilicon.c',
        ]
    endif

    if arch_arm and not get_option('jaguar-ns-decode').disabled()
        enable_jaguar_ns_decode = true
        enable_non_standard = true
        lib_sources += 'events-arch-arm/non-standard-jaguarmicro.c'
    endif

    if arch_x86 and not get_option('mce').disabled()
        enable_mce = true
        lib_sources += mce_sources
    endif

    if not get_option('memory-ce-pfa').disabled()
        enable_memory_ce_pfa = true
        enable_poison_page_stat = true
        enable_page_isolation = true
    endif

    if not get_option('memory-failure').disabled()
        enable_memory_failure = true
        enable_poison_page_stat = true
        lib_sources += 'events/ras-memory-failure-handler.c'
    endif

    if not get_option('memory-row-ce-pfa').disabled()
        enable_memory_row_ce_pfa = true
        enable_poison_page_stat = true
        enable_page_isolation = true
    endif

    if arch_arm and not get_option('nvidia-ns-decode').disabled()
        enable_nvidia_ns_decode = true
        enable_non_standard = true
        lib_sources += 'events-arch-arm/non-standard-nvidia.c'
    endif

    if not get_option('openbmc-unified-sel').disabled()
        enable_openbmc_unified_sel = true
        lib_sources += 'modules/unified-sel.c'
    endif

    if not get_option('signal').disabled()
        enable_signal = true
        lib_sources += 'events/ras-signal-handler.c'
    endif

    if arch_arm and not get_option('yitian-ns-decode').disabled()
        enable_yitian_ns_decode = true
        enable_non_standard = true
        lib_sources += 'events-arch-arm/non-standard-yitian.c'
    endif

    if arch_riscv and not get_option('reri').disabled()
        enable_reri = true
        lib_sources += 'events-arch-riscv/ras-reri-handler.c'
    endif

endif

#
# Non-standard (vendor-specific) CPER events
#

if enable_non_standard
    enable_non_standard = true
    lib_sources += 'events-arch-arm/ras-non-standard-handler.c'
endif

#
# Page isolation and memory poison
#

if enable_page_isolation
    lib_sources += [
        'core/rbtree.c',
        'modules/ras-page-isolation.c',
    ]
endif

if enable_poison_page_stat
    lib_sources += 'modules/ras-poison-page-stat.c'
endif

#
# Add database modules
#

if enable_db
    lib_sources += [
        'db/ras-db.c',
        'db/ras-record.c',
    ]
    unittest_sources += 'tests/test-databases.c'
endif

#
# Main program sources
#

rasdaemon_sources = ['core/rasdaemon.c']

# The production descriptors remain private.  Test-only accessors are built
# into the static library only when the C unit-test runner is available.
have_unittest = false
if not get_option('disable-all')
    cmocka_dep = dependency('cmocka', version: '>= 2.0', required: false)
    if cmocka_dep.found()
        c_flags += '-DCMOCKA_VERSION_2'
    else
        cmocka_dep = dependency('cmocka', required: false)
    endif
    if cmocka_dep.found()
        have_unittest = true
        c_flags += '-DHAVE_UNITTEST'
    endif
endif

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
    link_whole: [librasdaemon],
    c_args: c_flags,
    install: true,
    install_dir: get_option('sbindir'),
)

#
# Unit tests
#

if have_unittest
    unittest_exe = executable(
        'unittest', unittest_sources,
        dependencies: dependencies + cmocka_dep,
        link_whole: [librasdaemon],
        link_args: [
            '-Wl,--wrap=tep_get_field_raw',
            '-Wl,--wrap=tep_get_field_val',
            '-Wl,--wrap=tep_filter_match',
            '-Wl,--wrap=system',
        ],
        c_args: ['-g'] + c_flags,
    )

    # Keep the default Meson test set hermetic. The MySQL and PostgreSQL
    # groups remain real-server integration tests and are run explicitly by
    # the CI jobs which provision those servers.
    hermetic_test_groups = ['core', 'mc', 'modules']
    if enable_abrt_report
        hermetic_test_groups += 'report'
    endif
    if enable_aer
        hermetic_test_groups += 'aer'
    endif
    if enable_amp_ns_decode
        hermetic_test_groups += 'amp-ns'
    endif
    if enable_arm
        hermetic_test_groups += 'arm'
    endif
    if enable_cpu_fault_isolation
        hermetic_test_groups += 'cpu-isolation'
    endif
    if enable_cxl
        hermetic_test_groups += 'cxl'
    endif
    if enable_devlink
        hermetic_test_groups += 'devlink'
    endif
    if enable_diskerror
        hermetic_test_groups += 'diskerror'
    endif
    if enable_erst
        hermetic_test_groups += 'erst'
    endif
    if enable_extlog
        hermetic_test_groups += 'extlog'
    endif
    if enable_hisi_ns_decode
        hermetic_test_groups += 'hisi-ns'
    endif
    if enable_jaguar_ns_decode
        hermetic_test_groups += 'jaguar-ns'
    endif
    if enable_mce
        hermetic_test_groups += 'mce'
    endif
    if enable_memory_ce_pfa
        hermetic_test_groups += 'memory-ce-pfa'
    endif
    if enable_memory_failure
        hermetic_test_groups += 'memory-failure'
    endif
    if enable_memory_row_ce_pfa
        hermetic_test_groups += 'memory-row-ce-pfa'
    endif
    if enable_nvidia_ns_decode
        hermetic_test_groups += 'nvidia-ns'
    endif
    if enable_openbmc_unified_sel
        hermetic_test_groups += 'openbmc-sel'
    endif
    if enable_reri
        hermetic_test_groups += 'reri'
    endif
    if enable_signal
        hermetic_test_groups += 'signal'
    endif
    if enable_yitian_ns_decode
        hermetic_test_groups += 'yitian-ns'
    endif
    if enable_sqlite3
        hermetic_test_groups += 'sqlite3'
    endif

    if enable_db
        hermetic_test_groups += 'database'
    endif

    foreach test_group : hermetic_test_groups
        test(test_group, unittest_exe, args: ['--group', test_group])
    endforeach

endif

#
# Install files that don't require configuration
#

py.install_sources([
    'util/__init__.py',
    'util/display_table.py',
    'util/mem_layout.py',
    'util/ras_db.py',
    'util/ras_filter.py',
    'util/ras_dimm.py',
    'util/ras_env.py',
])

ras_mc_ctl_conf = configuration_data()
ras_mc_ctl_conf.set('PYTHON_DIR', pydir)

ras_mc_ctl = configure_file(
    input: 'util/ras-mc-ctl.py',
    output: 'ras-mc-ctl',
    configuration: ras_mc_ctl_conf,
    install: true,
    install_dir: get_option('sbindir'),
    install_mode: 'rwxr-xr-x',
)

rasdaemon_env = prefix / get_option('sysconfdir') / 'sysconfig' / 'rasdaemon'
install_data('misc/rasdaemon.env',
    install_dir: get_option('sysconfdir') + '/sysconfig',
    rename: 'rasdaemon',
)

install_data('contrib/mc_event_trigger',
    install_dir: get_option('sysconfdir') + '/ras/triggers',
    install_mode: 'rwxr-xr-x',
)

install_data('contrib/mem_fail_trigger',
    install_dir: get_option('sysconfdir') + '/ras/triggers',
    install_mode: 'rwxr-xr-x',
)

#
# Extra install files
#

conf_data = {
    # Basic package info
    'PACKAGE' : meson.project_name(),
    'PACKAGE_VERSION' : meson.project_version(),

    'prefix' : get_option('prefix'),
    'sysconfdir' : get_option('sysconfdir'),
    'localstatedir' : get_option('localstatedir'),
    'sbindir' : get_option('sbindir'),
    'SYSCONFDEFDIR' : get_option('sysconfdir') + '/sysconfig',

    'META_DATE' : run_command('date', '+%d %B %Y', check: true).stdout().strip(),
    'META_ALIAS' : meson.project_name(),

    'SQLITE3_DATABASE' : get_option('sqlite3-database'),

    'WITH_AER_TRUE' : enable_aer ? '' : '#',
    'WITH_ARM_TRUE' : enable_arm ? '' : '#',
    'WITH_CXL_TRUE' : enable_cxl ? '' : '#',
    'WITH_DEVLINK_TRUE' : enable_devlink ? '' : '#',
    'WITH_DISKERROR_TRUE' : enable_diskerror ? '' : '#',
    'WITH_EXTLOG_TRUE' : enable_extlog ? '' : '#',
    'WITH_MCE_TRUE' : enable_mce ? '' : '#',
    'WITH_MEMORY_FAILURE_TRUE' : enable_memory_failure ? '' : '#',
    'WITH_NVIDIA_NS_DECODE_TRUE' : enable_nvidia_ns_decode ? '' : '#',
    'WITH_SIGNAL_TRUE' : enable_signal ? '' : '#',

    'RASDAEMON_ENV': rasdaemon_env,
}

# Files requiring variable substitution and their install destinations.
# Value '' means: generate in build tree only (source-only, not installed).

install_map = {
    # Source-only
    'Makefile.inc.in' : '',
    'misc/rasdaemon.spec.in' : '',

    # Man pages
    'man/rasdaemon.1.in' : get_option('mandir') + '/man1',

    # Systemd service files
    'misc/rasdaemon.service.in' : systemd_unitdir,
    'misc/ras-mc-ctl.service.in' : systemd_unitdir,

    # Python config file
    'util/ras_config.py.in': pydir,

    # Other files
    'misc/rasdaemon.syslog-ng.in' : get_option('sysconfdir') + '/syslog-ng/conf.d',
    'misc/rasdaemon.rsyslog.in' : get_option('sysconfdir') + '/rsyslog.d',
    'misc/rasdaemon.logrotate.in' : get_option('sysconfdir') + '/logrotate.d',
}

# File renaming install rules

rename_map = {
    'rasdaemon.syslog-ng' : 'rasdaemon.conf',
    'rasdaemon.rsyslog' : 'rasdaemon.conf',
    'rasdaemon.logrotate' : 'rasdaemon',
}

foreach in_file, install_dir : install_map
    base = fs.name(in_file)
    out_file = base.strip('.in')

    # Determine install rename (if any)
    install_rename = out_file
    if rename_map.has_key(out_file)
        install_rename = rename_map[out_file]
    endif

    configured = configure_file(
        input: in_file,
        output: out_file,
        configuration: conf_data,
    )

    if install_dir != ''
        install_data(configured,
            install_dir: install_dir,
            rename: install_rename,
        )
    endif
endforeach

custom_target(
    'ras-mc-ctl-man',
    input: [
        ras_mc_ctl,
        'util/ras_db.py',
        'util/ras_filter.py',
        'util/ras_dimm.py',
    ],
    output: 'ras-mc-ctl.8',
    command: [
        py, files('util/help2man.py'),
        '--program', '@INPUT0@',
        '--output', '@OUTPUT@',
        '--python-dir', meson.current_build_dir(),
        '--source-dir', meson.current_source_dir() / 'util',
    ],
    build_by_default: true,
    install: true,
    install_dir: get_option('mandir') / 'man8',
)

# DIMM labels

labels_files = run_command(
    'find', 'labels', '-maxdepth', '1', '-type', 'f', check: true
).stdout().strip().split('\n')

labels_list = []

foreach f : labels_files
    if f != ''
        labels_list += f
    endif
endforeach

if labels_list.length() > 0
    install_data(labels_list,
        install_dir: get_option('sysconfdir') + '/ras/dimm_labels.d',
        install_mode: 'rw-r--r--',
    )
endif

# Shell completions

install_data('completions/ras-mc-ctl.bash',
    install_dir: bash_compdir,
    rename: 'ras-mc-ctl',
    install_mode: 'rw-r--r--',
)

install_data('completions/ras-mc-ctl.zsh',
    install_dir: zsh_compdir,
    rename: '_ras-mc-ctl',
    install_mode: 'rw-r--r--',
)

#
# Auto-generated config.h file
#

conf_header = configuration_data({
    # Keep in alphabetic order
    'HAVE_ABRT_REPORT' : enable_abrt_report,
    'HAVE_AER' : enable_aer,
    'HAVE_AMP_NS_DECODE' : enable_amp_ns_decode,
    'HAVE_ARM' : enable_arm,
    'HAVE_DB' : enable_db,
    'HAVE_CPU_FAULT_ISOLATION' : enable_cpu_fault_isolation,
    'HAVE_CXL' : enable_cxl,
    'HAVE_DEVLINK' : enable_devlink,
    'HAVE_DISKERROR' : enable_diskerror,
    'HAVE_ERST' : enable_erst,
    'HAVE_EXTLOG' : enable_extlog,
    'HAVE_HISI_NS_DECODE' : enable_hisi_ns_decode,
    'HAVE_JAGUAR_NS_DECODE' : enable_jaguar_ns_decode,
    'HAVE_MCE' : enable_mce,
    'HAVE_MEMORY_CE_PFA' : enable_memory_ce_pfa,
    'HAVE_MEMORY_FAILURE' : enable_memory_failure,
    'HAVE_MEMORY_ROW_CE_PFA' : enable_memory_row_ce_pfa,
    'HAVE_MYSQL' : enable_mysql,
    'HAVE_NON_STANDARD' : enable_non_standard,
    'HAVE_NVIDIA_NS_DECODE' : enable_nvidia_ns_decode,
    'HAVE_OPENBMC_UNIFIED_SEL' : enable_openbmc_unified_sel,
    'HAVE_POSTGRESQL' : enable_postgresql,
    'HAVE_RERI' : enable_reri,
    'HAVE_SIGNAL' : enable_signal,
    'HAVE_SQLITE3' : enable_sqlite3,
    'HAVE_YITIAN_NS_DECODE' : enable_yitian_ns_decode,
})

conf_header.set_quoted('RASDAEMON_ENV', rasdaemon_env)

configure_file(
    input : 'config.h.in',
    output : 'config.h',
    configuration : conf_header,
)

#
# Summary of enabled features
#

summary(
    {
        # Keep in alphabetic order
        'ABRT report' : enable_abrt_report,
        'AER' : enable_aer,
        'CPU fault isolation' : enable_cpu_fault_isolation,
        'CXL events' : enable_cxl,
        'DEVLINK' : enable_devlink,
        'Disk I/O errors' : enable_diskerror,
        'ERST' : enable_erst,
        'EXTLOG' : enable_extlog,
        'Memory CE PFA' : enable_memory_ce_pfa,
        'Memory Failure' : enable_memory_failure,
        'Memory ROW CE PFA' : enable_memory_row_ce_pfa,
        'OpenBMC unified' : enable_openbmc_unified_sel,
        'Signal' : enable_signal,
    },
    section : 'General options',
)

summary(
    {
        # Keep in alphabetic order
        'MySQL/MariaDB' : enable_mysql,
        'PostgreSQL' : enable_postgresql,
        'Sqlite3' : enable_sqlite3,
    },
    section : 'Database backends',
)

summary(
    {
        # Keep in alphabetic order
        'ARM events' : enable_arm,
        'MCE' : enable_mce,
        'RERI' : enable_reri,
    },
    section : 'Arch-specific options',
)

summary(
    {
        # Keep in alphabetic order
        'AMP RAS errors' : enable_amp_ns_decode,
        'HISI Kunpeng errors' : enable_hisi_ns_decode,
        'YITIAN RAS errors' : enable_yitian_ns_decode,
        'JAGUAR RAS errors' : enable_jaguar_ns_decode,
        'NVIDIA RAS errors' : enable_nvidia_ns_decode,
    },
    section : 'Non-standard CPER records',
)

summary(
    {
        'SQLite3 database': get_option('sqlite3-database'),
    },
    section : 'User defined options',
)
