#!/bin/bash

_ras_mc_ctl()
{
    local cur prev command word
    local global_opts="dimm mem database db -h --help -V --version -v --verbose -c --config"
    local dimm_opts="-h --help -m --mainboard -D --dmidecode -V --vendor -M --model -s --status -p --print-labels -g --guess-labels -r --register-labels -d --delay -L --labeldb -l --layout -e --error-count -P --per-rank -q --quiet"
    local database_opts="-h --help -v --verbose -e --errors -S --summary --table-summary -C --count -E --errors-per-table -L --list-tables -D --describe -I --create-index -s --since -u --until -H --hostname -w --where --module -x --select -g --group-by -o --order-by -c --corrected -U --uncorrected -d --deferred -f --fatal -i --info -r --recoverable -t --table -X --except --json"

    COMPREPLY=()
    cur="${COMP_WORDS[COMP_CWORD]}"
    prev="${COMP_WORDS[COMP_CWORD-1]}"

    case "$prev" in
        -c|--config|-L|--labeldb)
            COMPREPLY=($(compgen -f -- "$cur"))
            return
            ;;
        -d|--delay)
            COMPREPLY=($(compgen -W "1 5 10 30 60" -- "$cur"))
            return
            ;;
    esac

    for word in "${COMP_WORDS[@]:1:COMP_CWORD-1}"; do
        case "$word" in
            dimm|mem|database|db)
                command="$word"
                break
                ;;
        esac
    done

    case "$command" in
        dimm|mem)
            COMPREPLY=($(compgen -W "$dimm_opts" -- "$cur"))
            ;;
        database|db)
            COMPREPLY=($(compgen -W "$database_opts" -- "$cur"))
            ;;
        *)
            COMPREPLY=($(compgen -W "$global_opts" -- "$cur"))
            ;;
    esac
}

complete -F _ras_mc_ctl ras-mc-ctl
