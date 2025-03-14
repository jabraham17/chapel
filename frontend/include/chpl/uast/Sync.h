/*
 * Copyright 2021-2025 Hewlett Packard Enterprise Development LP
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

#ifndef CHPL_UAST_SYNC_H
#define CHPL_UAST_SYNC_H

#include "chpl/framework/Location.h"
#include "chpl/uast/AstNode.h"
#include "chpl/uast/SimpleBlockLike.h"

namespace chpl {
namespace uast {


/**
  This class represents a sync statement. For example:

  \rst
  .. code-block:: chapel

      // Example 1:
      proc syncExample() {
        sync {
          begin task1();
          begin task2();
        }
        writeln("Task 1 and 2 complete");
      }
      syncExample();

  \endrst

  This code will wait for both task1 and task2 to complete before printing
  due to use of the sync block.
 */

class Sync final : public SimpleBlockLike {
 friend class AstNode;

 private:
  Sync(AstList stmts, BlockStyle blockStyle, int bodyChildNum,
       int numBodyStmts)
    : SimpleBlockLike(asttags::Sync, std::move(stmts), blockStyle,
                      bodyChildNum,
                      numBodyStmts) {
    CHPL_ASSERT(bodyChildNum_ >= 0);
  }

  void serializeInner(Serializer& ser) const override {
    simpleBlockLikeSerializeInner(ser);
  }

  explicit Sync(Deserializer& des) : SimpleBlockLike(asttags::Sync, des) { }

  bool contentsMatchInner(const AstNode* other) const override {
    return simpleBlockLikeContentsMatchInner(other);
  }

  void markUniqueStringsInner(Context* context) const override {
    simpleBlockLikeMarkUniqueStringsInner(context);
  }

 public:
  ~Sync() override = default;

  /**
   Create and return a Sync containing the passed stmts.
   */
  static owned<Sync> build(Builder* builder, Location loc,
                           BlockStyle blockStyle,
                           AstList stmts);
};


} // end namespace uast
} // end namespace chpl

#endif
