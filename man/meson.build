# SPDX-License-Identifier: GPL-2.0-only
#
# Copyright (C) 2026 Mauro Carvalho Chehab <mchehab+huawei@kernel.org>

rasdaemon_man = configure_file(
    input: 'rasdaemon.8.in',
    output: 'rasdaemon.8',
    configuration: conf_data,
)
install_data(rasdaemon_man, install_dir: get_option('mandir') / 'man8')
