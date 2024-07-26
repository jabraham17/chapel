#!/usr/bin/env python3
import sys

import chpl_comm, chpl_network, overrides
from utils import memoize


@memoize
def get():
    substrate_val = overrides.get('CHPL_COMM_SUBSTRATE')
    if not substrate_val:
        comm_val = chpl_comm.get()
        network_val = chpl_network.get()

        if comm_val == 'gasnet':
            if network_val == 'aries':
                substrate_val = 'aries'
            elif network_val == 'slingshot':
                substrate_val = 'ofi'
            elif network_val == 'infiniband':
                substrate_val = 'ibv'
            else:
                substrate_val = 'udp'
        else:
            substrate_val = 'none'
    return substrate_val


def _main():
    substrate_val = get()
    sys.stdout.write("{0}\n".format(substrate_val))


if __name__ == '__main__':
    _main()
