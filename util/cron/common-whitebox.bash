#!/usr/bin/env bash
#
# Configure environment for a particular configuration for whitebox testing. To
# use this outside of nightly testing, set these two variables in the
# environment:
#
# Variable   Values
# ------------------------------------------------------
# COMPILER    cray, intel, pgi, gnu
# COMP_TYPE   TARGET, HOST-TARGET, HOST-TARGET-no-PrgEnv
#
# Optionally, the platform can be set with:
#
# CRAY_PLATFORM_FROM_JENKINS
#
# The default is cray-xc

UTIL_CRON_DIR=$(cd $(dirname ${BASH_SOURCE[0]}) ; pwd)
source $UTIL_CRON_DIR/functions.bash

# Ensure module commands are available.
local_bashrc=/etc/bash.bashrc.local
if [ -z "$(type module 2> /dev/null)" -a -f $local_bashrc ] ; then
    log_info "module command not available. Attempting to source ${local_bashrc}"
    source $local_bashrc
    if [ -z "$(type module 2> /dev/null)" ] ; then
        log_error "Could not access module command after sourceing local bashrc (${local_bashrc}). Exiting."
        exit 1
    fi
elif [ -z "$(type module 2> /dev/null)" ] ; then
    log_error "module command not available and local bashrc (${local_bashrc}) does not exist. Exiting."
    exit 2
fi

# Variable set by Jenkins to indicate type of whitebox. If it is not set, assume cray-xc.
platform=${CRAY_PLATFORM_FROM_JENKINS:-cray-xc}
log_info "Using platform: ${platform}"

short_platform=$(echo "${platform}" | cut -d- -f2)
log_info "Short platform: ${short_platform}"

# Setup vars that will help load the correct compiler module.
case $COMP_TYPE in
    TARGET)
        module_name=PrgEnv-${COMPILER}
        chpl_host_value=""

        export CHPL_TARGET_PLATFORM=$platform
        log_info "Set CHPL_TARGET_PLATFORM to: ${CHPL_TARGET_PLATFORM}"

        export CHPL_NIGHTLY_TEST_CONFIG_NAME="${short_platform}-wb.prgenv-${COMPILER}"
        ;;
    HOST-TARGET)
        module_name=PrgEnv-${COMPILER}
        chpl_host_value=cray-prgenv-${COMPILER}

        export CHPL_HOST_PLATFORM=$platform
        export CHPL_TARGET_PLATFORM=$platform
        log_info "Set CHPL_HOST_PLATFORM to: ${CHPL_HOST_PLATFORM}"
        log_info "Set CHPL_TARGET_PLATFORM to: ${CHPL_TARGET_PLATFORM}"

        export CHPL_NIGHTLY_TEST_CONFIG_NAME="${short_platform}-wb.host.prgenv-${COMPILER}"
        ;;
    HOST-TARGET-no-PrgEnv)
        the_cc=${COMPILER}
        if [ "${COMPILER}" = "gnu" ] ; then
            the_cc=gcc
        fi
        module_name=${the_cc}
        chpl_host_value=${COMPILER}

        export CHPL_NIGHTLY_TEST_CONFIG_NAME="${short_platform}-wb.${COMPILER}"
        ;;
    *)
        log_error "Unknown COMP_TYPE value: ${COMP_TYPE}. Exiting."
        exit 3
        ;;
esac

# load compiler versions from $CHPL_INTERNAL_REPO/build/compiler_versions.bash
# This should define load_target_compiler function and CHPL_GCC_TARGET_VERSION.
# The module uses the gen compiler to build the compiler and runtime, and the
# target version to test. For whitebox testing we use the target compiler for
# everything because there's no easy way to split up what we build with vs test
# with. We decided to always use the target compiler to get more exposure
# building with newer compilers.
source $CHPL_INTERNAL_REPO/build/compiler_versions.bash

# Then load the selected compiler
load_target_compiler ${COMPILER}

# Do minor fixups
case $COMPILER in
    cray|intel|gnu)
        # swap out network modules to get "host-only" environment
        log_info "Swap network module for host-only environment."
        module unload $(module -t list 2>&1 | grep craype-network)
        module load craype-network-none
        ;;
    pgi)
        # EJR (04/07/16): Since the default pgi was upgraded from 15.10.0 to
        # 16.3.0 on 04/02/16 the speculative gmp build gets stuck in an
        # infinite loop during `make check` while trying to test t_scan.c. Just
        # force disable gmp until there's more time to investigate this.
        export CHPL_GMP=none
        ;;
    *)
        log_error "Unknown COMPILER value: ${COMPILER}. Exiting."
        exit 4
        ;;
esac

log_info "Unloading cray-mpich module"
module unload cray-mpich

log_info "Unloading atp module"
module unload atp

export CHPL_HOME=$(cd $UTIL_CRON_DIR/../.. ; pwd)

# Set CHPL_HOST_COMPILER.
if [ -n "${chpl_host_value}" ] ; then
    export CHPL_HOST_COMPILER="${chpl_host_value}"
    log_info "Set CHPL_HOST_COMPILER to: ${CHPL_HOST_COMPILER}"
fi

# Disable launchers, comm.
export CHPL_LAUNCHER=none
export CHPL_COMM=none

# Disable llvm for now, since we're explicitly trying to test different C
# backends
export CHPL_LLVM=none


# Set some vars that nightly cares about.
export CHPL_NIGHTLY_LOGDIR=${CHPL_NIGHTLY_LOGDIR:-/hpcdc/project/chapel/Nightly}
export CHPL_NIGHTLY_CRON_LOGDIR="$CHPL_NIGHTLY_LOGDIR"

# Ensure compatible CPU targeting module is loaded. Unload any existing one
# (craype- that's not network/ hugepages) and load sandybridge, the LCD for XC
log_info "Loading craype-sandybridge."
module unload $(module -t list 2>&1 | grep craype- | grep -v network  | grep -v hugepage)
module load craype-sandybridge


log_info "Loading cray-fftw module."
module load cray-fftw

log_info "Current loaded modules:"
module list

log_info "Updating LD_LIBRARY_PATH to include CRAY_LD_LIBRARY_PATH"
export LD_LIBRARY_PATH=$CRAY_LD_LIBRARY_PATH:$LD_LIBRARY_PATH
echo $LD_LIBRARY_PATH

log_info "Chapel environment:"
$CHPL_HOME/util/printchplenv --all --no-tidy
