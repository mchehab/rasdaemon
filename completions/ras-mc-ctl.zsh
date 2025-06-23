#compdef ras-mc-ctl

_ras-mc-ctl() {
    local context state line
    typeset -A opt_args

    _arguments -C \
        '--quiet[Quiet operation]' \
        '--mainboard[Print mainboard vendor and model for this hardware]' \
        '--status[Print status of EDAC drivers]' \
        '--print-labels[Print Motherboard DIMM labels to stdout]' \
        '--guess-labels[Print DMI labels, when bank locator is available]' \
        '--register-labels[Load Motherboard DIMM labels into EDAC driver]' \
        '--delay=[Delay N seconds before writing DIMM labels]:delay (seconds):' \
        '--labeldb=[Load label database from file DB]:database file:_files' \
        '--layout[Display the memory layout]' \
        '--summary[Presents a summary of the logged errors]' \
        '--errors[Shows the errors stored at the error database]' \
        '--error-count[Shows the corrected and uncorrected error counts using sysfs]' \
        '--since=[Only include events since the date YYYY-MM-DD]:date (YYYY-MM-DD):_dates' \
        '--vendor-errors-summary[Presents a summary of the vendor-specific logged errors]:platform-id:' \
        '--vendor-errors[Shows the vendor-specific errors stored in the error database]:platform-id:' \
        '--vendor-platforms[List the supported platforms with platform-ids for the vendor-specific errors]' \
        '--help[This help message]' \
        && return 0

    return 1
}

_dates() {
    local -a dates
    local today=$(date +%Y-%m-%d)
    local yesterday=$(date -d "1 day ago" +%Y-%m-%d 2>/dev/null || date -v-1d +%Y-%m-%d 2>/dev/null)
    local week_ago=$(date -d "1 week ago" +%Y-%m-%d 2>/dev/null || date -v-1w +%Y-%m-%d 2>/dev/null)
    local month_ago=$(date -d "1 month ago" +%Y-%m-%d 2>/dev/null || date -v-1m +%Y-%m-%d 2>/dev/null)
    
    dates=(
        "$today:today"
        "$yesterday:yesterday" 
        "$week_ago:1 week ago"
        "$month_ago:1 month ago"
    )
    
    _describe 'date' dates
}

_ras-mc-ctl "$@"
