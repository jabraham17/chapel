#!/usr/bin/env bash
#
# Test default configuration on examples only, on linux64, with compiler gcc-10.1

UTIL_CRON_DIR=$(cd $(dirname ${BASH_SOURCE[0]}) ; pwd)
source $UTIL_CRON_DIR/common.bash

# Use CHPL_LLVM=none to avoid using a system LLVM potentially linked
# with a different and incompatible version of GCC
export CHPL_LLVM=none
export CHPL_LLVM_SUPPORT=bundled
unset CHPL_LLVM_CONFIG

source /hpcdc/project/chapel/setup_gcc.bash 10.1

# Set environment variables to nudge cmake towards GCC 10.1
export CHPL_CMAKE_USE_CC_CXX=1
export CC=$(which gcc)
export CXX=$(which g++)

gcc_version=$(gcc -dumpversion)
if [ "$gcc_version" != "10.1.0" ]; then
  echo "Wrong gcc version"
  echo "Expected Version: 10.1.0 Actual Version: $gcc_version"
  exit 2
fi

export CHPL_NIGHTLY_TEST_CONFIG_NAME="linux64-gcc101"

$UTIL_CRON_DIR/nightly -cron -examples -blog ${nightly_args}
