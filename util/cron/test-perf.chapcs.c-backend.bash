#!/usr/bin/env bash
#

UTIL_CRON_DIR=$(cd $(dirname ${BASH_SOURCE[0]}) ; pwd)

export CHPL_TEST_PERF_CONFIG_NAME='chapcs'

source $UTIL_CRON_DIR/common-perf.bash
source $UTIL_CRON_DIR/common-c-backend.bash

# common-llvm restricts us to extern/fergeson, we want all the perf tests
unset CHPL_NIGHTLY_TEST_DIRS

export CHPL_NIGHTLY_TEST_CONFIG_NAME="perf.chapcs.c-backend"

perf_args="-performance-description c-backend -performance-configs default:v,c-backend:v -sync-dir-suffix c-backend"
perf_args="${perf_args} -performance -numtrials 1 -startdate 11/24/15"

$UTIL_CRON_DIR/nightly -cron ${nightly_args} ${perf_args}
