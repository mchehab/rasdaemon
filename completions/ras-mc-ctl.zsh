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
                        '(-l --layout)'{-l,--layout}'[print the EDAC memory layout]'
                    ;;
                database|db)
                    _arguments \
                        '(-h --help)'{-h,--help}'[show command help]' \
                        '(--summary --list-tables --indexes-only)--errors[display detailed error records]' \
                        '(--errors --list-tables --indexes-only)--summary[display event counts by hostname and table]' \
                        '(--errors --summary --indexes-only)--list-tables[list discovered event tables]' \
                        '(--errors --summary --list-tables)--indexes-only[only create missing indexes]' \
                        '--since=[include records on or after this date]:date (YYYY-MM-DD):' \
                        '--until=[include records on or before this date]:date (YYYY-MM-DD):' \
                        '--hostname=[only include records for this hostname]:hostname:' \
                        '*--table=[include an exact table or shell-style pattern]:table pattern:' \
                        '*--except=[exclude an exact table or shell-style pattern]:table pattern:'
                    ;;
            esac
            ;;
    esac
}

_ras-mc-ctl "$@"
