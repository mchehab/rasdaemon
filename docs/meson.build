# SPDX-License-Identifier: GPL-2.0-only
#
# Copyright (C) 2026 Mauro Carvalho Chehab <mchehab+huawei@kernel.org>

sphinx = find_program('sphinx-build', required: false)

enable_sphinx = sphinx.found()
if not enable_sphinx
    message('Sphinx not found. Skipping documentation build')
else
    sphinx_sources += [
        'conf.py',
        'static/alabaster/custom.css',
        'static/furo/custom.css',
    ]

    core_headers = []
    foreach source : core_api_files
        header = fs.replace_suffix(source, '.h')
        if fs.is_file(meson.project_source_root() / header)
            core_headers += files('../' + header)
        endif
    endforeach

    custom_target(
        'sphinx',
        input: sphinx_sources + core_headers,
        output: 'html',
        command: [
            sphinx, '-q', '-W',
            '-D', 'version=' + meson.project_version(),
            '-D', 'release=' + meson.project_version(),
            '-b', 'html', meson.current_source_dir(),
            meson.project_build_root() / 'html',
        ],
        build_by_default: true,
        install: false,
    )
endif

summary(
    {'Build API docs': enable_sphinx},
    bool_yn: true,
    section: 'Documentation',
)
