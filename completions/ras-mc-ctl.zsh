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
                        '--error-count[display corrected and uncorrected DIMM error counts]' \
                        '(--error-count)--per-rank[show each rank separately]'
                    ;;
                database|db)
                    _arguments \
                        '(-h --help)'{-h,--help}'[show command help]' \
                        '(-v --verbose)'{-v,--verbose}'[describe tables selected for this query]' \
                        '(--summary --count -E --errors-per-table --list-tables --describe --create-index)--errors[display detailed error records]' \
                        '(--errors --count -E --errors-per-table --list-tables --describe --create-index)--summary[display event counts by hostname and table]' \
                        '(--errors --summary -E --errors-per-table --list-tables --describe --create-index)--count[count matching events]' \
                        '(--errors --summary --count --list-tables --describe --create-index)'{-E,--errors-per-table}'[count matching errors in each non-empty event table]' \
                        '(--errors --summary --count -E --errors-per-table --describe --create-index)--list-tables[list discovered event tables]' \
                        '(--errors --summary --count -E --errors-per-table --list-tables --create-index)--describe[describe selected event table fields]' \
                        '(--errors --summary --count -E --errors-per-table --list-tables --describe)--create-index[create missing indexes and exit]' \
                        '--since=[include records on or after this date]:date (YYYY-MM-DD):' \
                        '--until=[include records on or before this date]:date (YYYY-MM-DD):' \
                        '--hostname=[only include records for this hostname]:hostname:' \
                        '*--where=[require a field comparison]:filter:' \
                        '*--select=[display one field in detailed output]:field:' \
                        '*--group-by=[group count output by a field]:field:' \
                        '*--order-by=[order results]:field:' \
                        '(--uncorrected --deferred --fatal --info --recoverable)--corrected[select corrected errors]' \
                        '(--corrected --deferred --fatal --info --recoverable)--uncorrected[select uncorrected errors]' \
                        '(--corrected --uncorrected --fatal --info --recoverable)--deferred[select deferred errors]' \
                        '(--corrected --uncorrected --deferred --info --recoverable)--fatal[select fatal errors]' \
                        '(--corrected --uncorrected --deferred --fatal --recoverable)--info[select informational errors]' \
                        '(--corrected --uncorrected --deferred --fatal --info)--recoverable[select recoverable errors]' \
                        '*--table=[include an exact table or shell-style pattern]:table pattern:' \
                        '*--except=[exclude an exact table or shell-style pattern]:table pattern:'
                    ;;
            esac
            ;;
    esac
}

_ras-mc-ctl "$@"
