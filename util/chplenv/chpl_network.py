#!/usr/bin/env python3
import sys

import chpl_platform, overrides, chpl_comm_substrate
from utils import memoize, error

def get_valid_networks():
    return ['none', 'unset', 'slingshot', 'infiniband', 'ethernet', 'efa', 'aries']

@memoize
def get():
    network_val = overrides.get('CHPL_NETWORK')
    if not network_val:
        platform_val = chpl_platform.get('target')
        # Use aries on cray-xc series
        if platform_val == 'cray-xc':
            network_val = 'aries'
        # Use slingshot on hpe-cray-ex
        elif platform_val == 'hpe-cray-ex':
            network_val = 'slingshot'
        # Use infiniband on cray-cs, hpe-apollo, and pwr6
        elif platform_val in ('cray-cs', 'hpe-apollo', 'pwr6'):
            network_val = 'infiniband'
        else:
            # if a user has explicitly requested a comm value, see if we can infer a network value
            comm_val = overrides.get('CHPL_COMM')
            if not comm_val:
                # could not infer a network value
                network_val = 'unset'
            else:
                if comm_val == 'none':
                    network_val = 'none'
                elif comm_val == 'ugni':
                    network_val = 'aries'
                elif comm_val == 'gasnet':
                    # check the substrate value set by the user
                    substrate_val = overrides.get('CHPL_COMM_SUBSTRATE')
                    if substrate_val == 'aries':
                        network_val = 'aries'
                    elif substrate_val == 'ofi':
                        network_val = 'slingshot'
                    elif substrate_val == 'ibv':
                        network_val = 'infiniband'
                    elif substrate_val == 'udp':
                        network_val = 'ethernet'
                    elif substrate_val == 'smp':
                        # TODO:
                        pass
                    else:
                        network_val = 'unset'

                elif comm_val == 'ofi':
                    # we could query CHPL_RT_COMM_OFI_PROVIDER or FI_PROVIDER,
                    # but for now we'll just say 'unset'
                    network_val = 'unset'
                else:
                    network_val = 'unset'
    else:
        valid_networks = get_valid_networks()
        if network_val not in valid_networks:
            error("CHPL_NETWORK must be one of {0}\n".format(valid_networks))
    return network_val

@memoize
def is_set():
    return overrides.get('CHPL_NETWORK') is not None

def _main():
    val = get()
    sys.stdout.write("{0}\n".format(val))


if __name__ == '__main__':
    _main()
