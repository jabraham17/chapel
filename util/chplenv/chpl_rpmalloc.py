#!/usr/bin/env python3
import os
import sys
import optparse

import chpl_mem, overrides, third_party_utils
from utils import error, memoize


@memoize
def get(flag='target'):
    if flag == 'host':
        error("rpmalloc is not yet supported for host builds")
        return 'error'


    rpmalloc = 'none'
    if flag == 'target':
        # rpmalloc is only used if explicitly requested
        var = 'CHPL_TARGET_RPMALLOC'
        rpmalloc = overrides.get(var)

        mem = chpl_mem.get(flag)
        if rpmalloc is None:
            # if its none, we set it based on chpl_mem
            if mem == 'rpmalloc':
                rpmalloc = 'bundled'
            else:
                rpmalloc = 'none'
        else:
            # if mem is rpmalloc, then rpmalloc cannot be none
            if mem == 'rpmalloc' and rpmalloc == 'none':
                memvar = 'CHPL_{}_MEM'.format(flag.upper())
                error("{0} must not be 'none' when CHPL_TARGET_MEM is 'rpmalloc'".format(var, memvar))

    else:
        error("Invalid flag: '{0}'".format(flag), ValueError)

    supported_rpmalloc = ('none', 'bundled')
    if rpmalloc not in supported_rpmalloc:
        var = 'CHPL_{0}_RPMALLOC'.format(flag.upper())
        error("{0}={1} is not supported, must be one of {2}".format(var, rpmalloc, supported_rpmalloc))

    return rpmalloc



@memoize
def get_uniq_cfg_path(flag):
    # uses host/ or target/ before the usual subdir
    if flag == 'host':
        error("rpmalloc is not yet supported for host builds")
        return 'error'
    else:
        # add target/ before the usual subdir
        return os.path.join('target', third_party_utils.default_uniq_cfg_path())


# flag is host or target
# returns 2-tuple of lists
#  (compiler_bundled_args, compiler_system_args)
@memoize
def get_compile_args(flag):
    rpmalloc_val = get(flag)
    if rpmalloc_val == 'bundled':
        ucp_val = get_uniq_cfg_path(flag)
        return third_party_utils.get_bundled_compile_args('rpmalloc',
                                                          ucp=ucp_val)
    return ([ ], [ ])

# flag is host or target
# returns 2-tuple of lists
#  (linker_bundled_args, linker_system_args)
@memoize
def get_link_args(flag):
    rpmalloc_val = get(flag)
    if rpmalloc_val == 'bundled':
        ucp = get_uniq_cfg_path(flag)
        install_path = third_party_utils.get_bundled_install_path('rpmalloc',
                                                                  ucp=ucp)
        lib_path = os.path.join(install_path, 'lib')
        libs = ['-L{}'.format(lib_path), '-lrpmalloc']
        return (libs, [])
    return ([ ], [ ])

def _main():
    parser = optparse.OptionParser(usage='usage: %prog [--host|target])')
    parser.add_option('--target', dest='flag', action='store_const',
                      const='target', default='target')
    (options, args) = parser.parse_args()

    rpmalloc_val = get(options.flag)
    sys.stdout.write("{0}\n".format(rpmalloc_val))


if __name__ == '__main__':
    _main()
