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

#include "llvmParallelCodegen.h"

#ifdef HAVE_LLVM

#include "codegen.h"
#include "driver.h"
#include "expr.h"
#include "global-ast-vecs.h"
#include "llvmVer.h"
#include "misc.h"
#include "stmt.h"
#include "symbol.h"

#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/StringSet.h"
#include "llvm/Analysis/TargetTransformInfo.h"
#include "llvm/Bitcode/BitcodeReader.h"
#include "llvm/Bitcode/BitcodeWriter.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Pass.h"
#include "llvm/Support/CodeGen.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/MemoryBufferRef.h"
#include "llvm/Support/Threading.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/thread.h"
#include "llvm/Target/TargetMachine.h"
#if HAVE_LLVM_VER >= 170
#include "llvm/TargetParser/Triple.h"
#else
#include "llvm/ADT/Triple.h"
#endif

#include <algorithm>
#include <memory>
#include <utility>

// Function and global variable attribute holding the partition number.
static const char* const kPartitionAttr = "chpl-codegen-partition";

static unsigned partitionOf(llvm::Attribute attr) {
  unsigned part = 0;
  if (attr.isValid()) attr.getValueAsString().getAsInteger(10, part);
  return part;
}

unsigned getCodegenPartition(const llvm::Function& F) {
  return partitionOf(F.getFnAttribute(kPartitionAttr));
}

// Unnamed-address constants are copied into each partition that uses them
// instead of being shared.
static bool isDuplicatedConstant(const llvm::GlobalVariable& G) {
  return G.hasLocalLinkage() && G.isConstant() && G.hasGlobalUnnamedAddr() &&
         !G.isThreadLocal();
}

// Checks that the module can be split and that its local symbols can be
// given linkonce_odr linkage so that other partitions can reference them.
static bool canPartition(const llvm::Module& M) {
  // linkonce_odr definitions are emitted as weak symbols
  llvm::Triple triple(M.getTargetTriple());
  if (!triple.isOSBinFormatELF() && !triple.isOSBinFormatMachO()) return false;

  if (!M.getModuleInlineAsm().empty()) return false;
  if (!M.alias_empty() || !M.ifunc_empty()) return false;
  if (!M.getComdatSymbolTable().empty()) return false;
  // e.g. llvm.used and llvm.global_ctors
  for (const llvm::GlobalVariable& G : M.globals()) {
    if (G.hasAppendingLinkage()) return false;
  }
  return true;
}

// Names of the functions and module-level variables generated from Chapel
// code, which are munged to not match names from other code.
static llvm::StringSet<> collectChapelSymbolNames() {
  llvm::StringSet<> names;
  for (const auto& entry : gGenInfo->functionCNameAstrToSymbol) {
    names.insert(entry.first);
  }
  forv_Vec(VarSymbol, var, gVarSymbols) {
    if (var->defPoint->parentExpr != rootModule->block &&
        toModuleSymbol(var->defPoint->parentSymbol)) {
      names.insert(var->cname);
    }
  }
  return names;
}

unsigned partitionModuleForCodegen(llvm::Module& M) {
  if (fParMake <= 1) return 1;

  // -time-passes uses global state that is not thread safe
  if (gCodegenGPU || fDynoGenLib || !llvm::llvm_is_multithreaded() ||
      llvm::TimePassesIsEnabled || !canPartition(M)) {
    return 1;
  }

  std::vector<std::pair<uint64_t, llvm::Function*>> fns;
  for (llvm::Function& F : M) {
    if (F.isDeclaration()) continue;
    uint64_t size = 1;
    for (const llvm::BasicBlock& BB : F) {
      // a blockaddress must be in the same object as its function
      if (BB.hasAddressTaken()) return 1;
      size += BB.size();
    }
    fns.emplace_back(size, &F);
  }

  unsigned numParts = fParMake;
  if (fns.size() < numParts) numParts = fns.size();
  if (numParts <= 1) return 1;

  // Largest first, each to the least loaded partition.
  std::stable_sort(fns.begin(), fns.end(),
                   [](const auto& a, const auto& b) {
                     return a.first > b.first;
                   });
  std::vector<uint64_t> load(numParts, 0);
  for (auto& [size, F] : fns) {
    auto least = std::min_element(load.begin(), load.end());
    *least += size;
    F->addFnAttr(kPartitionAttr, std::to_string(least - load.begin()));
  }

  unsigned nextPart = 0;
  for (llvm::GlobalVariable& G : M.globals()) {
    if (G.isDeclaration() || isDuplicatedConstant(G)) continue;
    G.addAttribute(kPartitionAttr, std::to_string(nextPart));
    nextPart = (nextPart + 1) % numParts;
  }

  // Hidden keeps local symbols out of dynamic symbol tables. Other local
  // symbols (e.g. static functions from C headers) could share a name with a
  // global symbol in another object, so they stay strong under a name that C
  // code cannot use.
  llvm::StringSet<> chapelNames = collectChapelSymbolNames();
  for (llvm::GlobalValue& GV : M.global_values()) {
    if (GV.isDeclaration() || !GV.hasLocalLinkage()) continue;
    if (auto G = llvm::dyn_cast<llvm::GlobalVariable>(&GV)) {
      if (isDuplicatedConstant(*G)) continue;
    }
    if (GV.hasName() && chapelNames.count(GV.getName())) {
      GV.setLinkage(llvm::GlobalValue::LinkOnceODRLinkage);
    } else {
      GV.setName(GV.hasName() ? GV.getName() + ".chpl" : "chpl.anon");
      GV.setLinkage(llvm::GlobalValue::ExternalLinkage);
    }
    GV.setVisibility(llvm::GlobalValue::HiddenVisibility);
    GV.setDSOLocal(true);
  }

  return numParts;
}

unsigned getCodegenPartitionCount(const llvm::Module& M) {
  unsigned maxPart = 0;
  for (const llvm::Function& F : M) {
    if (F.isDeclaration()) continue;
    maxPart = std::max(maxPart, getCodegenPartition(F));
  }
  return maxPart + 1;
}

void emitObjectFile(llvm::Module& M, llvm::TargetMachine& TM,
                    llvm::raw_pwrite_stream& out) {
#if HAVE_LLVM_VER >= 180
  llvm::CodeGenFileType fileType = llvm::CodeGenFileType::ObjectFile;
#else
  llvm::CodeGenFileType fileType = llvm::CGFT_ObjectFile;
#endif
  bool disableVerify = !developer;

  llvm::legacy::PassManager emitPM;
  emitPM.add(llvm::createTargetTransformInfoWrapperPass(
             TM.getTargetIRAnalysis()));
  TM.addPassesToEmitFile(emitPM, out, nullptr, fileType, disableVerify);
  emitPM.run(M);
}

static std::unique_ptr<llvm::TargetMachine>
cloneTargetMachine(const llvm::TargetMachine& TM) {
  return std::unique_ptr<llvm::TargetMachine>(
    TM.getTarget().createTargetMachine(
#if LLVM_VERSION_MAJOR >= 21
      TM.getTargetTriple(),
#else
      TM.getTargetTriple().str(),
#endif
      TM.getTargetCPU(), TM.getTargetFeatureString(), TM.Options,
      TM.getRelocationModel(), TM.getCodeModel(), TM.getOptLevel()));
}

// Removes the duplicated constants that this partition does not use.
static void removeUnusedLocalGlobals(llvm::Module& M) {
  bool changed = true;
  while (changed) {
    changed = false;
    for (llvm::GlobalVariable& G : llvm::make_early_inc_range(M.globals())) {
      if (!G.hasLocalLinkage()) continue;
      G.removeDeadConstantUsers();
      if (G.use_empty()) {
        G.eraseFromParent();
        changed = true;
      }
    }
  }
}

// Returns an error message on failure.
static std::string
writeFile(const std::string& filename,
          llvm::function_ref<void(llvm::raw_pwrite_stream&)> write) {
  std::error_code error;
  llvm::raw_fd_ostream out(filename, error, llvm::sys::fs::OF_None);
  if (error || out.has_error()) {
    return "Could not open output file " + filename;
  }
  write(out);
  out.close();
  if (out.has_error()) {
    out.clear_error();
    return "Could not write output file " + filename;
  }
  return "";
}

// Runs on a worker thread, so it reports errors by returning a message.
static std::string emitPartition(llvm::MemoryBufferRef bitcode, unsigned part,
                                 llvm::TargetMachine& TM,
                                 const std::string& objFilename,
                                 const std::string& bcFilename) {
  llvm::LLVMContext context;
  context.setDiscardValueNames(bcFilename.empty());

  auto modOrErr = llvm::getLazyBitcodeModule(bitcode, context);
  if (!modOrErr) {
    return "Could not load LLVM bitcode: " +
           llvm::toString(modOrErr.takeError());
  }
  std::unique_ptr<llvm::Module> M = std::move(*modOrErr);

  // Done before materializing so other partitions' bodies are never parsed.
  for (llvm::Function& F : *M) {
    if (F.isDeclaration()) continue;
    if (getCodegenPartition(F) != part) {
      F.deleteBody();
    }
  }
  for (llvm::GlobalVariable& G : M->globals()) {
    if (G.isDeclaration() || G.hasLocalLinkage()) continue;
    if (partitionOf(G.getAttribute(kPartitionAttr)) != part) {
      G.setInitializer(nullptr);
      G.setLinkage(llvm::GlobalValue::ExternalLinkage);
      G.clearMetadata();
    }
  }

  if (llvm::Error err = M->materializeAll()) {
    return "Could not load LLVM bitcode: " + llvm::toString(std::move(err));
  }
  removeUnusedLocalGlobals(*M);

  // before codegen, which modifies the IR
  if (!bcFilename.empty()) {
    std::string err = writeFile(bcFilename, [&](llvm::raw_pwrite_stream& out) {
      llvm::WriteBitcodeToFile(*M, out);
    });
    if (!err.empty()) return err;
  }

  return writeFile(objFilename, [&](llvm::raw_pwrite_stream& out) {
    emitObjectFile(*M, TM, out);
  });
}

void emitPartitionedObjectFiles(llvm::MemoryBufferRef bitcode,
                                const llvm::TargetMachine& TM,
                                const std::vector<std::string>& objFilenames,
                                const std::vector<std::string>& bcFilenames) {
  unsigned numParts = objFilenames.size();
  auto bcFilename = [&](unsigned i) {
    return bcFilenames.empty() ? std::string() : bcFilenames[i];
  };

  // A TargetMachine must not be used by more than one thread at a time.
  std::vector<std::unique_ptr<llvm::TargetMachine>> tms;
  for (unsigned i = 0; i < numParts; i++) {
    tms.push_back(cloneTargetMachine(TM));
  }

  std::vector<std::string> errors(numParts);
  std::vector<llvm::thread> threads;
  for (unsigned i = 1; i < numParts; i++) {
    threads.emplace_back([&, i] {
      errors[i] = emitPartition(bitcode, i, *tms[i], objFilenames[i],
                                bcFilename(i));
    });
  }
  errors[0] = emitPartition(bitcode, 0, *tms[0], objFilenames[0],
                            bcFilename(0));
  for (llvm::thread& t : threads) {
    t.join();
  }

  for (const std::string& err : errors) {
    if (!err.empty()) USR_FATAL("%s", err.c_str());
  }
}

#endif
