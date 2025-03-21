/*
 * Copyright 2020-2025 Hewlett Packard Enterprise Development LP
 * Copyright 2004-2019 Cray Inc.
 * Other additional copyright holders may be indicated within.
 *
 * The entirety of this work is licensed under the Apache License,
 * Version 2.0 (the "License"); you may not use this file except
 * in compliance with the License.
 *
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

//
// MPI-based out-of-band support for the OFI-based Chapel comm layer.
//

#include "chplrt.h"
#include "chpl-env-gen.h"

#include "chpl-comm.h"
#include "chpl-mem.h"
#include "chpl-mem-sys.h"
#include "chpl-gen-includes.h"
#include "chplsys.h"
#include "error.h"

#include <assert.h>
#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "comm-ofi-internal.h"


#define MPI_CHK(expr) CHK_EQ_TYPED(expr, MPI_SUCCESS, int, "d")


void chpl_comm_ofi_oob_init(void) {
  int size, rank;

  MPI_CHK(MPI_Init(NULL, NULL));

  MPI_CHK(MPI_Comm_rank(MPI_COMM_WORLD, &rank));
  chpl_nodeID = (c_nodeid_t) rank;

  MPI_CHK(MPI_Comm_size(MPI_COMM_WORLD, &size));
  chpl_numNodes = (int32_t) size;

  chpl_comm_oob = "MPI";
  DBG_PRINTF(DBG_OOB, "OOB %s init: node %" PRI_c_nodeid_t " of %" PRId32,
             chpl_comm_oob, chpl_nodeID, chpl_numNodes);
}


void chpl_comm_ofi_oob_fini(void) {
  DBG_PRINTF(DBG_OOB, "OOB finalize");

  int inited;
  MPI_CHK(MPI_Initialized(&inited));
  if (inited){
    MPI_CHK(MPI_Finalize());
  }
}


void chpl_comm_ofi_oob_barrier(void) {
  DBG_PRINTF(DBG_OOB, "OOB barrier");
  MPI_CHK(MPI_Barrier(MPI_COMM_WORLD));
}


void chpl_comm_ofi_oob_allgather(const void* mine, void* all, size_t size) {
  DBG_PRINTF(DBG_OOB, "OOB allGather: %zd", size);

  //
  // MPI provides an ordered allGather, so we don't have to use the
  // trick we use for other OOB implementations, where we build a
  // meta-payload which includes the node index and then scatter the
  // results ourself.  How civilized!
  //
  MPI_CHK(MPI_Allgather(mine, size, MPI_BYTE,
                        all, size, MPI_BYTE,
                        MPI_COMM_WORLD));
}


void chpl_comm_ofi_oob_bcast(void* buf, size_t size) {
  DBG_PRINTF(DBG_OOB, "OOB bcast: %zd", size);
  MPI_CHK(MPI_Bcast(buf, size, MPI_BYTE, 0, MPI_COMM_WORLD));
}

int chpl_comm_ofi_oob_locales_on_node(int *rank) {
  //
  // The MPI_Comm_split_type() call splits the specified input communicator
  // (MPI_COMM_WORLD) into one or more output communicators which, because of the
  // MPI_COMM_TYPE_SHARED argument, each refer to the shared-memory regions of the
  // job. Then it returns these communicators to all of the ranks, with all ranks in
  // a given shared-memory region getting the same one. The ranks on a node can then
  // communicate just among themselves using this resulting communicator. Note that
  // MPI_Comm_split_type() is a collective call -- it must be called by all members
  // of the to-be-split communicator (here, MPI_COMM_WORLD) cooperatively. It's like
  // a barrier, in other words. For our use, we just need to know its size, i.e.,
  // the number of MPI ranks (Chapel locales) on our node.
  //
  MPI_Comm nodeComm;
  MPI_CHK(MPI_Comm_split_type(MPI_COMM_WORLD, MPI_COMM_TYPE_SHARED, 0,
                              MPI_INFO_NULL, &nodeComm));
  int nodeSize;
  MPI_CHK(MPI_Comm_size(nodeComm, &nodeSize));
  MPI_CHK(MPI_Comm_free(&nodeComm));
  DBG_PRINTF(DBG_OOB, "MPI OOB locales on node: %d", nodeSize);
  if (rank != NULL) {
    *rank = -1;  // not implemented
  }
  return nodeSize;
}
