#compdef ras-mc-ctl

_ras-mc-ctl()
{
    local context state line
    typeset -A opt_args

    _arguments -C \
        '(-h --help)'{-h,--help}'[show command help]' \
        '(-V --version)'{-V,--version}'[show program version]' \
        '(-v --verbose)'{-v,--verbose}'[enable verbose output]' \
        '(-c --config)'{-c,--config}'[use an alternate configuration file]:configuration file:_files' \
        '1:command:->command' \
        '*::command arguments:->arguments'

    case $state in
        command)
            _values 'command' \
                'dimm[inspect DIMMs and EDAC]' \
                'mem[alias for dimm]' \
                'database[display recorded RAS events]' \
                'db[alias for database]'
            ;;
        arguments)
            case $words[1] in
                dimm|mem)
                    _arguments \
                        '(-h --help)'{-h,--help}'[show command help]' \
                        '(-m --mainboard)'{-m,--mainboard}'[print mainboard vendor and model]' \
                        '(-D --dmidecode)'{-D,--dmidecode}'[use dmidecode for DIMM information]' \
                        '(-V --vendor)'{-V,--vendor}'[override mainboard vendor]:vendor:' \
                        '(-M --model)'{-M,--model}'[override mainboard model]:model:' \
                        '(-s --status)'{-s,--status}'[print EDAC driver status]' \
                        '(-p --print-labels)'{-p,--print-labels}'[print DIMM labels]' \
                        '(-g --guess-labels)'{-g,--guess-labels}'[guess DIMM labels from DMI]' \
                        '(-r --register-labels)'{-r,--register-labels}'[register DIMM labels]' \
                        '(-d --delay)'{-d,--delay}'[delay label registration]:seconds:(1 5 10 30 60)' \
                        '(-L --labeldb)'{-L,--labeldb}'[use an alternate label database]:label database:_files' \
                        '(-l --layout)'{-l,--layout}'[print the EDAC memory layout]' \
                        '{-e,--error-count}[display corrected and uncorrected DIMM error counts]' \
                        '(-e --error-count){-P,--per-rank}[show each rank separately]'
                    ;;
                database|db)
                    _arguments \
                        '(-h --help)'{-h,--help}'[show command help]' \
                        '(-v --verbose)'{-v,--verbose}'[describe tables selected for this query]' \
                        '(-S --summary -C --count -E --errors-per-table -L --list-tables -D --describe -I --create-index)'{-e,--errors}'[display detailed error records]' \
                        '(-e --errors -C --count -E --errors-per-table -L --list-tables -D --describe -I --create-index)'{-S,--summary}'[display event counts by hostname and table]' \
                        '(-e --errors -S --summary -E --errors-per-table -L --list-tables -D --describe -I --create-index)'{-C,--count}'[count matching events]' \
                        '(-e --errors -S --summary -C --count -L --list-tables -D --describe -I --create-index)'{-E,--errors-per-table}'[count matching errors in each non-empty event table]' \
                        '(-e --errors -S --summary -C --count -E --errors-per-table -D --describe -I --create-index)'{-L,--list-tables}'[list discovered event tables]' \
                        '(-e --errors -S --summary -C --count -E --errors-per-table -L --list-tables -I --create-index)'{-D,--describe}'[describe selected event table fields]' \
                        '(-e --errors -S --summary -C --count -E --errors-per-table -L --list-tables -D --describe)'{-I,--create-index}'[create missing indexes and exit]' \
                        '{-s,--since}=[include records on or after this date]:date (YYYY-MM-DD):' \
                        '{-u,--until}=[include records on or before this date]:date (YYYY-MM-DD):' \
                        '{-H,--hostname}=[only include records for this hostname]:hostname:' \
                        '*'{-w,--where}'=[require a field comparison]:filter:' \
                        '*'{-x,--select}'=[display one field in detailed output]:field:' \
                        '*'{-g,--group-by}'=[group count output by a field]:field:' \
                        '*'{-o,--order-by}'=[order results]:field:' \
                        '(-U --uncorrected -d --deferred -f --fatal -i --info -r --recoverable)'{-c,--corrected}'[select corrected errors]' \
                        '(-c --corrected -d --deferred -f --fatal -i --info -r --recoverable)'{-U,--uncorrected}'[select uncorrected errors]' \
                        '(-c --corrected -U --uncorrected -f --fatal -i --info -r --recoverable)'{-d,--deferred}'[select deferred errors]' \
                        '(-c --corrected -U --uncorrected -d --deferred -i --info -r --recoverable)'{-f,--fatal}'[select fatal errors]' \
                        '(-c --corrected -U --uncorrected -d --deferred -f --fatal -r --recoverable)'{-i,--info}'[select informational errors]' \
                        '(-c --corrected -U --uncorrected -d --deferred -f --fatal -i --info)'{-r,--recoverable}'[select recoverable errors]' \
                        '*'{-t,--table}'=[include an exact table or shell-style pattern]:table pattern:' \
                        '*'{-X,--except}'=[exclude an exact table or shell-style pattern]:table pattern:'
                    ;;
            esac
            ;;
    esac
}

_ras-mc-ctl "$@"
