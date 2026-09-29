# SPDX-License-Identifier: GPL-2.0-only
#
# Copyright (C) 2026 Mauro Carvalho Chehab <mchehab+huawei@kernel.org>

project('rasdaemon', 'c',
        version: '1.0.0',
        license: 'GPL-2.0-only',
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

#
# As we need to setup cmocka version at config.h, check cmocka
# deps at the main file, keeping the remaining test-related
# build logic under tests.
#
unittest_sources = []

have_unittest = false
cmocka_version_2 = false
if not get_option('disable-all')
    cmocka_dep = dependency('cmocka', version: '>= 2.0', required: false)
    if cmocka_dep.found()
        cmocka_version_2 = true
    else
        cmocka_dep = dependency('cmocka', required: false)
    endif
    if cmocka_dep.found()
        have_unittest = true
        c_flags += '-DHAVE_UNITTEST'
    endif
endif

#
# Extra prefix directories
#

prefix  = get_option('prefix')
libdir  = get_option('libdir')
datadir = get_option('datadir')

rasdaemon_env = prefix / get_option('sysconfdir') / 'sysconfig' / 'rasdaemon'

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
    dependency('threads'),
]

#
# Core sources - They're also used when generating Sphinx docs
#
core_api_files = [
    'core/bitfield.c',
    'core/event-consumer.c',
    'core/modules.c',
    'core/ras-logger.c',
    'core/ras-env.c',
    'core/ras-events.c',
    'events/ras-mc-handler.c',
    'core/trigger.c',
    'core/types.c',
]

core_queue = ['core/queue.c']
core_rbtree = ['core/rbtree.c']

db_files = ['db/ras-db.c']

db_sqlite3 = ['db/db-sqlite3.c']
db_mysql = ['db/db-mysql.c']
db_postgresql = ['db/db-postgresql.c']

lib_sources = core_api_files

# C files that are documented on Sphinx
core_api_files += core_queue + core_rbtree
core_api_files +=  db_files + db_sqlite3 + db_mysql + db_postgresql

rasdaemon_sources = ['core/rasdaemon.c']

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
enable_ampere_oem_sel = false
enable_arm = false
enable_arm_vendor_data = false
enable_cpu_fault_isolation = false
enable_cxl = false
enable_db = false
enable_debug_sql = false
enable_devlink = false
enable_diskerror = false
enable_erst = false
enable_extlog = false
enable_hisi_ns_decode = false
enable_bmc_generic = false
enable_ipmi_bmc = false
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
enable_pcie_edpc = false
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
    mysql_dep = dependency('mysqlclient', 'mariadb', 'libmariadb',
                           required: get_option('mysql'))

    if mysql_dep.found() and not get_option('mysql').disabled()
        if cc.has_header('mysql/mysql.h')
            enable_db = true
            enable_mysql = true
            dependencies += mysql_dep
            lib_sources += db_mysql
            unittest_sources += files('tests/test-mysql.c')
        else
            name = mysql_dep.name()
            ver = mysql_dep.version()
            message(name + ' version ' + ver + ': mysql.h not found!')
        endif
    else
        message('MySQL client devel not found: disabling MySQL support')
    endif

    pq_dep = dependency('libpq', required: get_option('postgresql'))
    if pq_dep.found() and not get_option('postgresql').disabled()
        enable_db = true
        enable_postgresql = true
        lib_sources += db_postgresql
        unittest_sources += files('tests/test-postgresql.c')
        dependencies += pq_dep
    else
        message('libpq devel not found: disabling PostgreSQL support')
    endif

    sqlite3_dep = dependency('sqlite3', required: get_option('sqlite3'))
    if sqlite3_dep.found() and not get_option('sqlite3').disabled()
        enable_db = true
        enable_sqlite3 = true
        lib_sources += db_sqlite3
        unittest_sources += files('tests/test-sqlite3.c')
        dependencies += sqlite3_dep
    else
        message('SQLite3 devel not found: disabling SQLite3 support')
    endif

    libpci_dep = dependency('libpci', required: get_option('aer'))
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
        lib_sources += 'actions/abrt-report.c'
    endif

    if arch_arm and not get_option('amp-ns-decode').disabled()
        enable_amp_ns_decode = true
        enable_arm_vendor_data = true
        enable_non_standard = true
        lib_sources += [
            'events-arch-arm/non-standard-ampere.c',
            'events-arch-arm/non-standard-ampereone.c',
        ]
    endif

    if arch_arm and not get_option('ampere-oem-sel').disabled()
        enable_ampere_oem_sel = true
        enable_ipmi_bmc = true
        lib_sources += 'actions/ampere-oem-sel.c'
    endif

    if not get_option('bmc-generic').disabled()
        enable_bmc_generic = true
        enable_ipmi_bmc = true
        lib_sources += 'actions/bmc-generic.c'
    endif

    if enable_aer and not get_option('pcie-edpc').disabled()
        pcie_edpc_libpci_dep = dependency('libpci', version: '>=3.10.0',
                                          required: get_option('pcie-edpc'))
        if pcie_edpc_libpci_dep.found()
            enable_pcie_edpc = true
            lib_sources += 'actions/ras-pcie-edpc.c'
        else
            message('libpci >= 3.10.0 is required for PCIe eDPC; disabling it')
        endif
    endif

    if arch_arm and not get_option('openbmc-unified-sel').disabled()
        enable_openbmc_unified_sel = true
        enable_ipmi_bmc = true
        lib_sources += 'actions/openbmc-unified-sel.c'
    endif

    if arch_arm and not get_option('arm').disabled()
        enable_arm = true
        enable_arm_vendor_data = true
        lib_sources += 'events-arch-arm/ras-arm-handler.c'
    endif

    if not get_option('cpu-fault-isolation').disabled()
        enable_cpu_fault_isolation = true
        lib_sources += ['actions/ras-cpu-isolation.c'] + core_queue
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

    if arch_x86 and not get_option('erst').disabled() and not get_option('mce').disabled()
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
# SQL debug only makes sense if DB is selected
#
if get_option('debug-sql').enabled() and enable_db
    enable_debug_sql = true
endif

#
# ARM processor event vendor data
#

if enable_arm_vendor_data
    lib_sources += 'events-arch-arm/ras-arm-vendor-data.c'
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
    lib_sources += core_rbtree + ['actions/ras-page-isolation.c']
endif

if enable_poison_page_stat
    lib_sources += 'actions/ras-poison-page-stat.c'
endif

#
# Add database modules
#

if enable_ipmi_bmc
    lib_sources += 'actions/ipmi-bmc.c'
endif

if enable_db
    lib_sources += ['actions/db-store.c'] + db_files
    unittest_sources += files('tests/test-databases.c')
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

subdir('tests')

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

sphinx_sources = files(core_api_files) + files(rasdaemon_sources)

#
# Install files that don't require configuration
#


#
# Configuration data
#

conf_data = {
    # Basic package info
    'PACKAGE' : meson.project_name(),
    'PACKAGE_VERSION' : meson.project_version(),

    'prefix' : prefix,
    'sysconfdir' : prefix / get_option('sysconfdir'),
    'localstatedir' : prefix / get_option('localstatedir'),
    'sbindir' : prefix / get_option('sbindir'),
    'SYSCONFDEFDIR' : prefix / get_option('sysconfdir') / 'sysconfig',

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

#
# Use separate meson files for some of the directories
#

subdir('docs')
subdir('man')
subdir('util')
subdir('misc')
subdir('labels')
subdir('contrib')

# Keep the root-level source-only Makefile fragment in the root build dir.
configure_file(
    input: 'Makefile.inc.in',
    output: 'Makefile.inc',
    configuration: conf_data,
)

# Core Python configuration is generated at the build root for helper tools
# that add the build directory itself to PYTHONPATH.
ras_config = configure_file(
    input: 'util/ras_config.py.in',
    output: 'ras_config.py',
    configuration: conf_data,
)
install_data(ras_config, install_dir: pydir)

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
    'DEBUG_SQL' : enable_debug_sql,
    'HAVE_ABRT_REPORT' : enable_abrt_report,
    'HAVE_AER' : enable_aer,
    'HAVE_AMP_NS_DECODE' : enable_amp_ns_decode,
    'HAVE_AMPERE_OEM_SEL' : enable_ampere_oem_sel,
    'HAVE_ARM' : enable_arm,
    'HAVE_DB' : enable_db,
    'HAVE_CPU_FAULT_ISOLATION' : enable_cpu_fault_isolation,
    'HAVE_CXL' : enable_cxl,
    'HAVE_DEVLINK' : enable_devlink,
    'HAVE_DISKERROR' : enable_diskerror,
    'HAVE_ERST' : enable_erst,
    'HAVE_EXTLOG' : enable_extlog,
    'HAVE_HISI_NS_DECODE' : enable_hisi_ns_decode,
    'HAVE_BMC_GENERIC' : enable_bmc_generic,
    'HAVE_IPMI_BMC' : enable_ipmi_bmc,
    'HAVE_JAGUAR_NS_DECODE' : enable_jaguar_ns_decode,
    'HAVE_MCE' : enable_mce,
    'HAVE_MEMORY_CE_PFA' : enable_memory_ce_pfa,
    'HAVE_MEMORY_FAILURE' : enable_memory_failure,
    'HAVE_MEMORY_ROW_CE_PFA' : enable_memory_row_ce_pfa,
    'HAVE_MYSQL' : enable_mysql,
    'HAVE_NON_STANDARD' : enable_non_standard,
    'HAVE_NVIDIA_NS_DECODE' : enable_nvidia_ns_decode,
    'HAVE_OPENBMC_UNIFIED_SEL' : enable_openbmc_unified_sel,
    'HAVE_PCIE_EDPC' : enable_pcie_edpc,
    'HAVE_POSTGRESQL' : enable_postgresql,
    'HAVE_RERI' : enable_reri,
    'HAVE_SIGNAL' : enable_signal,
    'HAVE_SQLITE3' : enable_sqlite3,
    'HAVE_YITIAN_NS_DECODE' : enable_yitian_ns_decode,
})

conf_header.set_quoted('RASDAEMON_ENV', rasdaemon_env)

configure_file(
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
        'Ampere OEM SEL' : enable_ampere_oem_sel,
        'CPU fault isolation' : enable_cpu_fault_isolation,
        'CXL events' : enable_cxl,
        'DEVLINK' : enable_devlink,
        'Disk I/O errors' : enable_diskerror,
        'ERST' : enable_erst,
        'EXTLOG' : enable_extlog,
        'Facebook Unified SEL' : enable_openbmc_unified_sel,
        'Generic BMC SEL'     : enable_bmc_generic,
        'Memory CE PFA' : enable_memory_ce_pfa,
        'Memory Failure' : enable_memory_failure,
        'Memory ROW CE PFA' : enable_memory_row_ce_pfa,
        'PCIe eDPC' : enable_pcie_edpc,
        'Signal' : enable_signal,
    },
    bool_yn: true,
    section : 'General options',
)

summary(
    {
        # Keep in alphabetic order
        'MySQL/MariaDB' : enable_mysql,
        'PostgreSQL' : enable_postgresql,
        'Sqlite3' : enable_sqlite3,
    },
    bool_yn: true,
    section : 'Database backends',
)

summary(
    {
        # Keep in alphabetic order
        'ARM events' : enable_arm,
        'MCE' : enable_mce,
        'RERI' : enable_reri,
    },
    bool_yn: true,
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
    bool_yn: true,
    section : 'Non-standard CPER records',
)

summary(
    {
        # Keep in alphabetic order
        'SQLite3 database file': get_option('sqlite3-database'),
    },
    bool_yn: true,
    section : 'User defined options',
)
