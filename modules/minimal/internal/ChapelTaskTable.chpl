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

// ChapelTaskTable.chpl
//

module ChapelTaskTable {
  proc chpldev_taskTable_init() {
  }

  export proc chpldev_taskTable_add(taskID   : chpl_taskID_t,
                                    lineno   : uint(32),
                                    filename : chpl__c_void_ptr,
                                    tl_info  : uint(64))
  {
  }

  export proc chpldev_taskTable_remove(taskID : chpl_taskID_t)
  {
  }

  export proc chpldev_taskTable_set_active(taskID : chpl_taskID_t)
  {
  }

  export proc chpldev_taskTable_set_suspended(taskID : chpl_taskID_t)
  {
  }

  export proc chpldev_taskTable_get_tl_info(taskID : chpl_taskID_t)
  {
  }

  export proc chpldev_taskTable_print()
  {
  }
}
