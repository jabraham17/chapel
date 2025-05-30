#!/usr/bin/env python3
import optparse
import sys

import chpl_comm, chpl_compiler, chpl_platform, overrides
from compiler_utils import CompVersion, get_compiler_version, has_std_atomics
from utils import error, memoize, warning, check_valid_var


@memoize
def get(flag='target'):
    if flag == 'network':
        valid = ['none', 'ofi', 'ugni']
        atomics_val = overrides.get('CHPL_NETWORK_ATOMICS')
        comm_val = chpl_comm.get()
        if not atomics_val:
            if comm_val in ['ofi', 'ugni'] and get('target') != 'locks':
                atomics_val = comm_val
            else:
                atomics_val = 'none'
        elif atomics_val == 'gasnet':
            error("CHPL_NETWORK_ATOMICS=gasnet is not supported")
        elif atomics_val not in valid:
            check_valid_var("CHPL_NETWORK_ATOMICS", atomics_val, valid)
        elif atomics_val != 'none' and atomics_val != comm_val:
            error("CHPL_NETWORK_ATOMICS=%s is incompatible with CHPL_COMM=%s"
                  % (atomics_val, comm_val))
    elif flag == 'target':
        atomics_val = overrides.get('CHPL_ATOMICS')
        is_user_specified = atomics_val is not None
        is_intel = False
        if not atomics_val:
            compiler_val = chpl_compiler.get('target')
            platform_val = chpl_platform.get('target')

            # We default to C standard atomics (cstdlib) for gcc 5 and newer.
            # Some prior versions of gcc look like they support standard
            # atomics, but have buggy or missing parts of the implementation,
            # so we do not try to use cstdlib with gcc < 5. If support is
            # detected for clang (via preprocessor checks) we also default to
            # cstdlib atomics. For llvm-clang we always default to
            # cstdlib atomics. We know the llvm-clang will have compiler
            # support for atomics and llvm requires gcc 4.8 (or a compiler with
            # equivalent features) to be built so we know we'll have system
            # header support too.
            #
            # We support intrinsics for gcc, intel, cray and clang. gcc added
            # initial support in 4.1, and added support for 64-bit atomics on
            # 32-bit platforms with 4.8. clang and intel also support 64-bit
            # atomics on 32-bit platforms and the cray compiler will never run
            # on a 32-bit machine.
            #
            # For pgi or 32-bit platforms with an older gcc, we fall back to
            # locks

            is_intel = compiler_val == 'intel' or compiler_val == 'cray-prgenv-intel'
            if compiler_val in ['gnu', 'cray-prgenv-gnu', 'mpi-gnu']:
                version = get_compiler_version(flag)
                if version >= CompVersion('5.0'):
                    atomics_val = 'cstdlib'
                elif version >= CompVersion('4.8'):
                    atomics_val = 'intrinsics'
                elif version >= CompVersion('4.1') and not platform_val.endswith('32'):
                    atomics_val = 'intrinsics'
            elif is_intel:
                atomics_val = 'intrinsics'
            elif compiler_val == 'cray-prgenv-cray':
                atomics_val = 'cstdlib'
            elif compiler_val in ['allinea', 'cray-prgenv-allinea']:
                atomics_val = 'cstdlib'
            elif compiler_val == 'clang':
                # if using bundled LLVM, we can use cstdlib atomics
                import chpl_llvm
                has_llvm = chpl_llvm.get()
                if has_llvm == "bundled" or has_std_atomics():
                    atomics_val = 'cstdlib'
                else:
                    atomics_val = 'intrinsics'
            elif compiler_val == 'llvm':
                atomics_val = 'cstdlib'

            # we can't use intrinsics, fall back to locks
            if not atomics_val:
                atomics_val = 'locks'

        check_valid_var("CHPL_ATOMICS", atomics_val, ['cstdlib', 'intrinsics', 'locks'])

    else:
        error("Invalid flag: '{0}'".format(flag), ValueError)

    if flag == 'target' and atomics_val == 'intrinsics':
        msg = "Using CHPL_ATOMICS=intrinsics is a known performance issue"
        if is_intel:
            msg += " but is required for portability with Intel compilers for the time being"
        elif is_user_specified:
            msg += ": please consider using CHPL_ATOMICS=cstdlib"
        warning(msg)

    if flag == 'target' and atomics_val == 'locks':
        platform_val = chpl_platform.get('target')
        if platform_val == 'darwin':
            error("CHPL_ATOMICS=locks is not supported on MacOS")

    return atomics_val


def _main():
    parser = optparse.OptionParser(usage='usage: %prog [--network|target])')
    parser.add_option('--target', dest='flag', action='store_const',
                      const='target', default='target')
    parser.add_option('--network', dest='flag', action='store_const',
                      const='network')
    (options, args) = parser.parse_args()

    atomics_val = get(options.flag)
    sys.stdout.write("{0}\n".format(atomics_val))


if __name__ == '__main__':
    _main()
