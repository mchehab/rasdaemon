# SPDX-License-Identifier: GPL-2.0-only
#
# Copyright (C) 2026 Mauro Carvalho Chehab <mchehab+huawei@kernel.org>

unittest_sources += [
    'main.c',
    'trace-mock.c',
    'test-db-cli.c',
    'test-core.c',
    'test-events.c',
    'test-modules.c',
    'test-db-concurrency.c',
]

if have_unittest
    test_runner_c_flags = c_flags
    if cmocka_version_2
        test_runner_c_flags += '-DCMOCKA_VERSION_2'
    endif

    unittest_exe = executable(
        'unittest', unittest_sources,
        include_directories: include_directories('..'),
        dependencies: dependencies + cmocka_dep,
        link_whole: [librasdaemon],
        link_args: [
            '-Wl,--wrap=tep_get_field_raw',
            '-Wl,--wrap=tep_get_field_val',
            '-Wl,--wrap=tep_filter_match',
            '-Wl,--wrap=system',
            '-Wl,--wrap=popen',
            '-Wl,--wrap=pclose',
            '-Wl,--wrap=access',
        ],
        c_args: ['-g',
                 '-DRAS_SOURCE_DIR="' + meson.project_source_root() + '"',
                 '-DRAS_BUILD_DIR="' + meson.project_build_root() + '"'] + test_runner_c_flags,
    )

    # Keep the default Meson test set hermetic. The MySQL and PostgreSQL
    # groups remain real-server integration tests and are run explicitly by
    # the CI jobs which provision those servers.
    hermetic_test_groups = ['core', 'events', 'modules']
    if enable_mce
        hermetic_test_groups += 'x86-events'
    endif
    if enable_arm or enable_amp_ns_decode or enable_hisi_ns_decode or enable_jaguar_ns_decode or enable_nvidia_ns_decode or enable_yitian_ns_decode
        hermetic_test_groups += 'arm-events'
    endif
    if enable_reri
        hermetic_test_groups += 'riscv-events'
    endif
    if enable_abrt_report or enable_cpu_fault_isolation or \
       enable_memory_ce_pfa or enable_memory_row_ce_pfa or enable_ipmi_bmc or \
       enable_pcie_edpc
        hermetic_test_groups += 'actions'
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
