# Install DIMM label files.
labels_files = run_command(
    'find', meson.current_source_dir(), '-maxdepth', '1', '-type', 'f', check: true
).stdout().strip().split('\n')

labels_list = []

foreach f : labels_files
    if f != '' and f != 'meson.build'
        labels_list += f
    endif
endforeach

if labels_list.length() > 0
    install_data(labels_list,
        install_dir: get_option('sysconfdir') / 'ras/dimm_labels.d',
        install_mode: 'rw-r--r--',
    )
endif
