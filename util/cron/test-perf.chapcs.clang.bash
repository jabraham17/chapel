#!/usr/bin/env bash
#

UTIL_CRON_DIR=$(cd $(dirname ${BASH_SOURCE[0]}) ; pwd)

export CHPL_TEST_PERF_CONFIG_NAME='chapcs'

source $UTIL_CRON_DIR/common-perf.bash

# Load LLVM Spack install to get clang in PATH
eval `$CHPL_DEPS_SPACK_ROOT/bin/spack --env chpl-base-deps load --sh llvm`
unset CC
unset CXX

export CHPL_HOST_COMPILER=clang
export CHPL_TARGET_COMPILER=clang

export CHPL_NIGHTLY_TEST_CONFIG_NAME="perf.chapcs.clang"

SHORT_NAME=clang
START_DATE=09/10/16

perf_args="-performance-description $SHORT_NAME -performance-configs default:v,$SHORT_NAME:v -sync-dir-suffix $SHORT_NAME"
perf_args="${perf_args} -performance -numtrials 1 -startdate $START_DATE"

$UTIL_CRON_DIR/nightly -cron ${nightly_args} ${perf_args}
