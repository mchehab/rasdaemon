#!/bin/sh
#  This shell script can be executed by rasdaemon in daemon mode when a
#  memory_failure_event is occured, environment variables include all
#  information reported by tracepoint.
#

echo TIMESTAMP: $TIMESTAMP
echo PFN: $PFN
echo PAGE_TYPE: $PAGE_TYPE
echo ACTION_RESULT: $ACTION_RESULT

exit 0
