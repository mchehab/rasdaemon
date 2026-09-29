# SPDX-License-Identifier: GPL-2.0-only
#
# Copyright (C) 2026 Mauro Carvalho Chehab <mchehab+huawei@kernel.org>

py.install_sources([
    '__init__.py',
    'display_table.py',
    'mem_layout.py',
    'ras_db.py',
    'ras_db_decode.py',
    'ras_filter.py',
    'ras_dimm.py',
    'ras_env.py',
])

ras_mc_ctl_conf = configuration_data()
ras_mc_ctl_conf.set('PYTHON_DIR', pydir)

ras_mc_ctl = configure_file(
    input: 'ras-mc-ctl.py',
    output: 'ras-mc-ctl',
    configuration: ras_mc_ctl_conf,
    install: true,
    install_dir: get_option('sbindir'),
    install_mode: 'rwxr-xr-x',
)

custom_target(
    'ras-mc-ctl-man',
    input: [
        ras_mc_ctl,
        'ras_db.py',
        'ras_db_decode.py',
        'ras_filter.py',
        'ras_dimm.py',
    ],
    output: 'ras-mc-ctl.8',
    command: [
        py, files('help2man.py'),
        '--program', '@INPUT0@',
        '--output', '@OUTPUT@',
        '--python-dir', meson.project_build_root(),
        '--source-dir', meson.current_source_dir(),
        '--sysconf-dir', prefix / get_option('sysconfdir'),
    ],
    build_by_default: true,
    install: true,
    install_dir: get_option('mandir') / 'man8',
)
