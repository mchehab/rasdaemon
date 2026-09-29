# SPDX-License-Identifier: GPL-2.0-only
#
# Copyright (C) 2026 Mauro Carvalho Chehab <mchehab+huawei@kernel.org>

# Package spec is generated for source packaging, not installed.
configure_file(
    input: 'rasdaemon.spec.in',
    output: 'rasdaemon.spec',
    configuration: conf_data,
)

install_data('rasdaemon.env',
    install_dir: get_option('sysconfdir') / 'sysconfig',
    rename: 'rasdaemon',
)

misc_install_map = {
    'rasdaemon.service.in': systemd_unitdir,
    'ras-mc-ctl.service.in': systemd_unitdir,
    'rasdaemon.syslog-ng.in': get_option('sysconfdir') / 'syslog-ng/conf.d',
    'rasdaemon.rsyslog.in': get_option('sysconfdir') / 'rsyslog.d',
    'rasdaemon.logrotate.in': get_option('sysconfdir') / 'logrotate.d',
}

misc_rename_map = {
    'rasdaemon.syslog-ng': 'rasdaemon.conf',
    'rasdaemon.rsyslog': 'rasdaemon.conf',
    'rasdaemon.logrotate': 'rasdaemon',
}

foreach in_file, install_dir : misc_install_map
    out_file = in_file.strip('.in')
    install_rename = misc_rename_map.get(out_file, out_file)

    configured = configure_file(
        input: in_file,
        output: out_file,
        configuration: conf_data,
    )

    install_data(configured,
        install_dir: install_dir,
        rename: install_rename,
    )
endforeach
