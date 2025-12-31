#!/bin/bash

_ras_mc_ctl() {
    local cur prev opts
    COMPREPLY=()
    cur="${COMP_WORDS[COMP_CWORD]}"
    prev="${COMP_WORDS[COMP_CWORD-1]}"

    opts="--quiet --mainboard --status --print-labels --guess-labels --register-labels 
          --delay --labeldb --layout --summary --errors --error-count --since 
          --vendor-errors-summary --vendor-errors --vendor-platforms --help"

    case "${prev}" in
        --delay)
            COMPREPLY=($(compgen -W "1 5 10 30 60" -- ${cur}))
            return 0
            ;;
        --labeldb)
            COMPREPLY=($(compgen -f -- ${cur}))
            return 0
            ;;
        --since)
            local today=$(date +%Y-%m-%d)
            local yesterday=$(date -d "1 day ago" +%Y-%m-%d 2>/dev/null || date -v-1d +%Y-%m-%d 2>/dev/null || echo "")
            local week_ago=$(date -d "1 week ago" +%Y-%m-%d 2>/dev/null || date -v-1w +%Y-%m-%d 2>/dev/null || echo "")
            local month_ago=$(date -d "1 month ago" +%Y-%m-%d 2>/dev/null || date -v-1m +%Y-%m-%d 2>/dev/null || echo "")
            
            local dates="$today"
            [[ -n "$yesterday" ]] && dates="$dates $yesterday"
            [[ -n "$week_ago" ]] && dates="$dates $week_ago"
            [[ -n "$month_ago" ]] && dates="$dates $month_ago"
            
            COMPREPLY=($(compgen -W "$dates" -- ${cur}))
            return 0
            ;;
        --vendor-errors-summary|--vendor-errors)
            COMPREPLY=()
            return 0
            ;;
    esac

    if [[ ${cur} == --*=* ]]; then
        local option=${cur%%=*}
        local value=${cur#*=}
        
        case "${option}" in
            --delay)
                local suggestions="1 5 10 30 60"
                COMPREPLY=($(compgen -W "$suggestions" -P "${option}=" -- ${value}))
                return 0
                ;;
            --labeldb)
                COMPREPLY=($(compgen -f -P "${option}=" -- ${value}))
                return 0
                ;;
            --since)
                local today=$(date +%Y-%m-%d)
                local yesterday=$(date -d "1 day ago" +%Y-%m-%d 2>/dev/null || date -v-1d +%Y-%m-%d 2>/dev/null || echo "")
                local week_ago=$(date -d "1 week ago" +%Y-%m-%d 2>/dev/null || date -v-1w +%Y-%m-%d 2>/dev/null || echo "")
                local month_ago=$(date -d "1 month ago" +%Y-%m-%d 2>/dev/null || date -v-1m +%Y-%m-%d 2>/dev/null || echo "")
                
                local dates="$today"
                [[ -n "$yesterday" ]] && dates="$dates $yesterday"
                [[ -n "$week_ago" ]] && dates="$dates $week_ago"
                [[ -n "$month_ago" ]] && dates="$dates $month_ago"
                
                COMPREPLY=($(compgen -W "$dates" -P "${option}=" -- ${value}))
                return 0
                ;;
        esac
    fi

    COMPREPLY=($(compgen -W "${opts}" -- ${cur}))
    return 0
}

complete -F _ras_mc_ctl ras-mc-ctl
