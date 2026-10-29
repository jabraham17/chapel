/*
 * Copyright 2026 Hewlett Packard Enterprise Development LP
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

#ifndef _LLVM_PARALLEL_CODEGEN_H_
#define _LLVM_PARALLEL_CODEGEN_H_

#ifdef HAVE_LLVM

#include <string>
#include <vector>

namespace llvm {
  class Function;
  class MemoryBufferRef;
  class Module;
  class TargetMachine;
  class raw_pwrite_stream;
}

// Assigns every definition in 'M' to a code generation partition and gives
// local symbols hidden non-local linkage so that partitions can reference
// each other: linkonce_odr for Chapel symbols, and a renamed external symbol
// otherwise. Only splits when requested with '-j'. Returns the number of
// partitions; 1 means 'M' is unchanged.
unsigned partitionModuleForCodegen(llvm::Module& M);

// Returns the partition that generates code for 'F'.
unsigned getCodegenPartition(const llvm::Function& F);

// Returns the number of partitions recorded by partitionModuleForCodegen.
// 'M' may be lazily loaded.
unsigned getCodegenPartitionCount(const llvm::Module& M);

// Generates an object file for 'M'.
void emitObjectFile(llvm::Module& M, llvm::TargetMachine& TM,
                    llvm::raw_pwrite_stream& out);

// Generates the object file for each partition of the module in 'bitcode'
// in parallel. Partition i is written to objFilenames[i], and its bitcode to
// bcFilenames[i] if 'bcFilenames' is not empty.
void emitPartitionedObjectFiles(llvm::MemoryBufferRef bitcode,
                                const llvm::TargetMachine& TM,
                                const std::vector<std::string>& objFilenames,
                                const std::vector<std::string>& bcFilenames);

#endif

#endif
