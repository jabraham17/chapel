#!/usr/bin/env bash
#
# Run GPU performance testing on a Cray EX

UTIL_CRON_DIR=$(cd $(dirname ${BASH_SOURCE[0]}) ; pwd)
source $UTIL_CRON_DIR/common-native-gpu.bash
source $UTIL_CRON_DIR/common-hpe-cray-ex.bash

module load cuda/12.4

export CHPL_LLVM=system
export CHPL_COMM=none
export CHPL_LOCALE_MODEL=gpu
export CHPL_LAUNCHER_PARTITION=griz256
export CHPL_GPU=nvidia  # amd is detected automatically
export CHPL_GPU_MEM_STRATEGY=unified_memory

export CHPL_NIGHTLY_TEST_CONFIG_NAME="perf.gpu-ex-cuda-12.um"

export CHPL_TEST_PERF_CONFIG_NAME="1-node-a100" # pinoak has ampere GPUs
source $UTIL_CRON_DIR/common-native-gpu-perf.bash
# make sure this comes after setting SUBDIR (set by native-gpu-perf) and
# CONFIG_NAME
source $UTIL_CRON_DIR/common-perf.bash

SHORT_NAME=um
nightly_args="${nightly_args} -performance-description $SHORT_NAME -performance-configs default:v,$SHORT_NAME:v -sync-dir-suffix $SHORT_NAME"
nightly_args="${nightly_args} -startdate 10/10/24"

$UTIL_CRON_DIR/nightly -cron ${nightly_args}
