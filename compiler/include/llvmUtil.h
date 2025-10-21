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

#ifndef _LLVMUTIL_H_
#define _LLVMUTIL_H_

#ifdef HAVE_LLVM

#include "llvmVer.h"
#include "llvm/IR/IRBuilder.h"

#include <utility>
#include <type_traits>

struct PromotedPair {
  llvm::Value* a;
  llvm::Value* b;
  bool isSigned;
  PromotedPair(llvm::Value* a, llvm::Value* b, bool isSigned)
    : a(a), b(b), isSigned(isSigned)
  {
  }
};

bool isArrayVecOrStruct(llvm::Type* t);


template<typename Ty>
constexpr static bool is_Inst_or_BBiterator() {
  return (std::is_pointer_v<Ty> &&
          std::is_base_of_v<llvm::Instruction, std::remove_pointer_t<Ty>>) ||
          std::is_same_v<llvm::BasicBlock::iterator, Ty>;
}

#if HAVE_LLVM_VER >= 200
template <typename Ty,
          std::enable_if_t<is_Inst_or_BBiterator<Ty>(),  bool> = true>
static llvm::BasicBlock::iterator getInsertPosition(Ty I) {
  return llvm::BasicBlock::iterator(I);
}
#else
template <typename Ty,
          std::enable_if_t<is_Inst_or_BBiterator<Ty>(),  bool> = true>
static llvm::Instruction* getInsertPosition(Ty I) {
  if constexpr (std::is_same_v<llvm::BasicBlock::iterator, Ty>) {
    return &*I;
  } else {
    return I;
  }
}
#endif

// align=0 means undefined alignment; this does not follow 'AlignmentStatus'
// creates an alloca instruction and inserts it before insertBefore
llvm::AllocaInst* makeAlloca(llvm::Type* type, const char* name, llvm::Instruction* insertBefore, unsigned n=1, unsigned align=0);

// creates an alloca instruction at the top of the function
// must be followed by setValueAlignment()
// todo: add Type* and Symbol* args to createAllocaInFunctionEntry instead
llvm::AllocaInst* createAllocaInFunctionEntry(llvm::IRBuilder<>* irBuilder, llvm::Type* type, const char* name);

PromotedPair convertValuesToLarger(llvm::IRBuilder<> *irBuilder, llvm::Value *value1, llvm::Value *value2, bool isSigned1 = false, bool isSigned2 = false);
llvm::Value *convertValueToType(llvm::IRBuilder<>* irBuilder,
                                const llvm::DataLayout& layout,
                                llvm::LLVMContext &ctx,
                                llvm::Value *value, llvm::Type *newType,
                                llvm::AllocaInst **alloca, // an alloca generated
                                bool isSigned=false, bool force=false);

void makeLifetimeStart(llvm::IRBuilder<>* irBuilder,
                       const llvm::DataLayout& layout,
                       llvm::LLVMContext &ctx,
                       llvm::Type *valType, llvm::Value *addr);

int64_t getTypeSizeInBytes(const llvm::DataLayout& layout, llvm::Type* ty);
bool isTypeSizeSmallerThan(const llvm::DataLayout& layout, llvm::Type* ty, uint64_t max_size_bytes);

void print_llvm(llvm::Type* t);
void print_llvm(llvm::Value* v);
void print_llvm(llvm::Module* m);
void print_llvm(llvm::Metadata* m);

// print_clang() is available in clangUtil.h,cpp

void list_view(const llvm::Type* t);
void list_view(const llvm::Value* v);
void list_view(const llvm::Module* m);
void list_view(const llvm::Metadata* m);

void nprint_view(const llvm::Type* t);
void nprint_view(const llvm::Value* v);
void nprint_view(const llvm::Module* m);
void nprint_view(const llvm::Metadata* m);

llvm::AttrBuilder llvmPrepareAttrBuilder(llvm::LLVMContext& ctx);

void llvmAddAttr(llvm::LLVMContext& ctx, llvm::AttributeList& attrs,
                 size_t idx,
                 llvm::AttrBuilder& b);

void llvmAttachStructRetAttr(llvm::AttrBuilder& b, llvm::Type* returnTy);

bool isOpaquePointer(llvm::Type* ty);

// if ptr is an AllocaInst or GlobalValue, we can get the pointed-to type
// from it. Otherwise, return nullptr.
llvm::Type* tryComputingPointerElementType(llvm::Value* ptr);

// These functions return the LLVM equivalent of a `void*`
// Newer LLVMs don't distinguish between pointer type, these return an opaque `ptr`
llvm::Type* getPointerType(llvm::LLVMContext& ctx, unsigned AS=0);
llvm::Type* getPointerType(llvm::IRBuilder<>* irBuilder, unsigned AS=0);
// although this takes a type, it returns an opaque `ptr` on newer LLVMs
llvm::Type* getPointerType(llvm::Type* eltType, unsigned AS=0);

#endif //HAVE_LLVM
#endif //LLVMUTIL_H
