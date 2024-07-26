#!/usr/bin/env python3
import sys

import chpl_network, overrides
from utils import memoize


@memoize
def get():
    comm_val = overrides.get('CHPL_COMM')
    if not comm_val:
        network_val = chpl_network.get()
        if network_val == 'aries':
            comm_val = 'ugni'
        elif network_val == 'slingshot':
            comm_val = 'ofi'
        elif network_val == 'infiniband':
            comm_val = 'gasnet'
        elif network_val == 'ethernet':
            comm_val = 'gasnet'
        elif network_val == 'efa':
            comm_val = 'ofi'
        else:
            comm_val = 'none'
    return comm_val


def _main():
    comm_val = get()
    sys.stdout.write("{0}\n".format(comm_val))


if __name__ == '__main__':
    _main()
