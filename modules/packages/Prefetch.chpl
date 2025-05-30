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

/**
 *  Prefetch
 */
module Prefetch {
  private use CTypes;

  import Prefetch_internal;
  inline proc prefetch(addr:c_ptr) {
    Prefetch_internal.chpl_prefetch(addr:c_ptr(void));
  }

  inline proc prefetch(addr:c_ptr(void)) {
    Prefetch_internal.chpl_prefetch(addr);
  }
}

module Prefetch_internal {
  use CTypes;
  extern proc chpl_prefetch(addr: c_ptr(void));
}
