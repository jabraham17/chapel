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

#include "chplrt.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "chpl-comm.h"
#include "chpl-mem.h"
#include "chplmemtrack.h"
#include "chpltypes.h"
#include "error.h"

#include <mimalloc.h>

enum heap_type {FIXED, DYNAMIC, NONE};

static struct shared_heap {
  enum heap_type type;
  void* base;
  size_t size;
  size_t cur_offset;
  pthread_mutex_t alloc_lock;
} heap;

void chpl_mem_layerInit(void) {
  void* heap_base;
  size_t heap_size;

  chpl_comm_regMemHeapInfo(&heap_base, &heap_size);
  if (heap_base != NULL && heap_size == 0) {
    chpl_internal_error("if heap address is specified, size must be also");
  }

  if (heap_base != NULL) {
    heap.type = FIXED;
    heap.base = heap_base;
    heap.size = heap_size;
    heap.cur_offset = 0;
    if (pthread_mutex_init(&heap.alloc_lock, NULL) != 0) {
      chpl_internal_error("cannot init chunk_alloc lock");
    }


    if (!mi_manage_os_memory(heap.base, heap.size, false, false, false, -1)) {
      chpl_internal_error("mi_manage_os_memory failed");
    }
    mi_option_set(mi_option_disallow_os_alloc, 1);
    mi_option_set(mi_option_limit_os_alloc, 1);

  } else {
    // assume nothing to do
  }


}


void chpl_mem_layerExit(void) { }
