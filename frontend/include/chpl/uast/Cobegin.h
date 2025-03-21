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

#ifndef CHPL_UAST_COBEGIN_H
#define CHPL_UAST_COBEGIN_H

#include "chpl/framework/Location.h"
#include "chpl/uast/BlockStyle.h"
#include "chpl/uast/AstNode.h"
#include "chpl/uast/SimpleBlockLike.h"
#include "chpl/uast/WithClause.h"

namespace chpl {
namespace uast {


/**
  This class represents a cobegin statement. For example:

  \rst
  .. code-block:: chapel

      // Example 1:
      var x = 0;
      cobegin {
        writeln(x);
      }

  \endrst

 */
class Cobegin final : public AstNode {
 friend class AstNode;

 private:
  int8_t withClauseChildNum_;
  int bodyChildNum_;
  int numTaskBodies_;

  Cobegin(AstList children, int8_t withClauseChildNum, int bodyChildNum,
          int numTaskBodies)
    : AstNode(asttags::Cobegin, std::move(children)),
      withClauseChildNum_(withClauseChildNum),
      bodyChildNum_(bodyChildNum),
      numTaskBodies_(numTaskBodies) {
  }

  void serializeInner(Serializer& ser) const override {
    ser.write(withClauseChildNum_ );
    ser.writeVInt(bodyChildNum_);
    ser.writeVInt(numTaskBodies_);
  }

  explicit Cobegin(Deserializer& des) : AstNode(asttags::Cobegin, des) {
    withClauseChildNum_ = des.read<int8_t>();
    bodyChildNum_ = des.readVInt();
    numTaskBodies_ = des.readVInt();
  }

  bool contentsMatchInner(const AstNode* other) const override {
    const Cobegin* lhs = this;
    const Cobegin* rhs = (const Cobegin*) other;

    if (lhs->withClauseChildNum_ != rhs->withClauseChildNum_)
      return false;

    if (lhs->bodyChildNum_ != rhs->bodyChildNum_)
      return false;

    if (lhs->numTaskBodies_ != rhs->numTaskBodies_)
      return false;

    return true;
  }

  void markUniqueStringsInner(Context* context) const override {
  }

  std::string dumpChildLabelInner(int i) const override;

 public:

  /**
    Create and return a cobegin statement.
  */
  static owned<Cobegin> build(Builder* builder, Location loc,
                              owned<WithClause> withClause,
                              AstList taskBodies);

  /**
    Returns the with clause of this cobegin statement, or nullptr if there
    is none.
  */
  const WithClause* withClause() const {
    if (withClauseChildNum_ < 0) return nullptr;
    auto ret = child(withClauseChildNum_);
    CHPL_ASSERT(ret->isWithClause());
    return (const WithClause*)ret;
  }

  /**
    Return a way to iterate over the task bodies.
   */
  AstListIteratorPair<AstNode> taskBodies() const {
    auto begin = children_.begin() + bodyChildNum_;
    auto end = begin + numTaskBodies_;
    return AstListIteratorPair<AstNode>(begin, end);
  }

  /**
    Return the number of task bodies in this.
  */
  int numTaskBodies() const {
    return this->numTaskBodies_;
  }

  /**
    Return the i'th task body in this.
  */
  const AstNode* taskBody(int i) const {
    CHPL_ASSERT(i >= 0 && i < numTaskBodies_);
    const AstNode* ast = this->child(i + bodyChildNum_);
    return ast;
  }
};


} // end namespace uast
} // end namespace chpl

#endif
