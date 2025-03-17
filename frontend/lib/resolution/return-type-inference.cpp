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

#include "return-type-inference.h"

#include "chpl/framework/ErrorMessage.h"
#include "chpl/framework/UniqueString.h"
#include "chpl/framework/global-strings.h"
#include "chpl/framework/query-impl.h"
#include "chpl/parsing/parsing-queries.h"
#include "chpl/resolution/ResolvedVisitor.h"
#include "chpl/resolution/can-pass.h"
#include "chpl/resolution/disambiguation.h"
#include "chpl/resolution/intents.h"
#include "chpl/resolution/resolution-queries.h"
#include "chpl/resolution/scope-queries.h"
#include "chpl/types/all-types.h"
#include "chpl/uast/all-uast.h"

#include "Resolver.h"

#include <cstdio>
#include <iterator>
#include <set>
#include <string>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

namespace chpl {
namespace resolution {


using namespace uast;
using namespace types;

// forward declarations
static QualifiedType adjustForReturnIntent(uast::Function::ReturnIntent ri,
                                           QualifiedType retType);


/* pair (interface ID, implementation point ID) */
using ImplementedInterface =
  std::pair<const InterfaceType*, ID>;

/* (parent class, implemented interfaces) tuple resulting from processing
   the inheritance expressions of a class/record. */
using InheritanceExprResolutionResult =
  std::pair<const types::BasicClassType*, std::vector<ImplementedInterface>>;

static const InheritanceExprResolutionResult&
processInheritanceExpressionsForAggregateQuery(Context* context,
                                               const AggregateDecl* ad,
                                               SubstitutionsMap substitutions,
                                               const PoiScope* poiScope) {
  QUERY_BEGIN(processInheritanceExpressionsForAggregateQuery, context, ad, substitutions, poiScope);
  const BasicClassType* parentClassType = nullptr;
  const AstNode* parentClassNode = nullptr;
  std::vector<ImplementedInterface> implementationPoints;
  auto c = ad->toClass();

  for (auto inheritExpr : ad->inheritExprs()) {
    // Resolve the parent class type expression
    ResolutionResultByPostorderID r;
    auto visitor =
      Resolver::createForParentClass(context, ad, inheritExpr,
                                     substitutions,
                                     poiScope, r);
    inheritExpr->traverse(visitor);

    auto& rr = r.byAst(inheritExpr);
    QualifiedType qt = rr.type();
    const BasicClassType* newParentClassType = nullptr;
    if (auto t = qt.type()) {
      if (auto bct = t->toBasicClassType()) {
        newParentClassType = bct;
      } else if (auto ct = t->toClassType()) {
        // safe because it's checked for null later.
        newParentClassType = ct->basicClassType();
      }
    }

    bool foundParentClass = qt.isType() && newParentClassType != nullptr;
    if (!c && foundParentClass) {
      CHPL_REPORT(context, NonClassInheritance, ad, inheritExpr, newParentClassType);
    } else if (foundParentClass) {
      // It's a valid parent class; is it the only one? (error otherwise).
      if (parentClassType) {
        CHPL_ASSERT(parentClassNode);
        reportInvalidMultipleInheritance(context, c, parentClassNode, inheritExpr);
      } else {
        parentClassType = newParentClassType;
        parentClassNode = inheritExpr;
      }

      // OK
    } else if (qt.isType() && qt.type() && qt.type()->isInterfaceType()) {
      auto ift = qt.type()->toInterfaceType();
      if (!ift->substitutions().empty()) {
        context->error(inheritExpr, "cannot specify instantiated interface type in inheritance expression");
      } else {
        implementationPoints.emplace_back(ift, inheritExpr->id());
      }
    } else {
      context->error(inheritExpr, "invalid parent class expression");
      parentClassType = BasicClassType::getRootClassType(context);
      parentClassNode = inheritExpr;
    }
  }

  InheritanceExprResolutionResult result {
    parentClassType, std::move(implementationPoints)
  };
  return QUERY_END(result);
}

static const std::vector<const ImplementationPoint*>&
getImplementedInterfacesQuery(Context* context,
                              const AggregateDecl* ad) {
  QUERY_BEGIN(getImplementedInterfacesQuery, context, ad);
  std::vector<const ImplementationPoint*> result;
  std::map<const InterfaceType*, ID> seen;
  auto inheritanceResult =
    processInheritanceExpressionsForAggregateQuery(context, ad, {}, nullptr);
  auto& implementationPoints = inheritanceResult.second;

  auto initialType = QualifiedType(QualifiedType::TYPE,
                                   initialTypeForTypeDecl(context, ad->id()));

  for (auto& implementedInterface : implementationPoints) {
    auto insertionResult = seen.insert({ implementedInterface.first, implementedInterface.second });

    if (!insertionResult.second) {
      // We already saw an 'implements' for this interface
      CHPL_REPORT(context, InterfaceMultipleImplements, ad,
                  implementedInterface.first, insertionResult.first->second,
                  implementedInterface.second);
    } else {
      auto ift = InterfaceType::withTypes(context, implementedInterface.first,
                                          { initialType });
      if (!ift) {
        // we gave it a single type, but got null back, which means it's
        // not a unary interface.
        CHPL_REPORT(context, InterfaceNaryInInherits, ad,
                    implementedInterface.first, implementedInterface.second);
      } else {
        auto implPoint =
          ImplementationPoint::get(context, ift, implementedInterface.second);
        result.push_back(implPoint);
      }
    }
  };

  return QUERY_END(result);
}

const std::vector<const ImplementationPoint*>&
getImplementedInterfaces(Context* context,
                         const AggregateDecl* ad) {
  return getImplementedInterfacesQuery(context, ad);
}

// Get a Type for an AggregateDecl
// poiScope, instantiatedFrom are nullptr if not instantiating
const CompositeType* helpGetTypeForDecl(Context* context,
                                        const AggregateDecl* ad,
                                        const SubstitutionsMap& substitutions,
                                        const PoiScope* poiScope,
                                        const Type* instantiatedFrom) {

  CHPL_ASSERT(ad);

  // Filter out substitutions that aren't fields within 'ad'.
  // In particular, there might be substitutions to do with a parent class.
  SubstitutionsMap filteredSubs;
  for (const auto& pair : substitutions) {
    if (ad->id().contains(pair.first)) {
      filteredSubs.insert(pair);
    }
  }

  if (filteredSubs.empty()) {
    instantiatedFrom = nullptr;
  }

  const CompositeType* ret = nullptr;

  if (const Class* c = ad->toClass()) {
    const BasicClassType* parentClassType =
      processInheritanceExpressionsForAggregateQuery(context, ad,
                                                     substitutions,
                                                     poiScope).first;

    // All the parent expressions could've been interfaces, and we just
    // inherit from object.
    if (!parentClassType) {
      parentClassType = BasicClassType::getRootClassType(context);
    }

    if (!parentClassType->isObjectType() && !substitutions.empty()) {
      // recompute the parent class type with substitutions
      auto parentAst = parsing::idToAst(context, parentClassType->id());
      CHPL_ASSERT(parentAst);
      auto parentAd = parentAst->toAggregateDecl();
      CHPL_ASSERT(parentAd);
      auto got = helpGetTypeForDecl(context,
                                    parentAd,
                                    substitutions,
                                    poiScope,
                                    parentClassType);
      auto gotBct = got->toBasicClassType();
      CHPL_ASSERT(gotBct);
      parentClassType = gotBct;
    }

    // Elsewhere, we use 'instantiatedFrom' to signal a generic instantiation,
    // even if the filtered substitutions are empty. Keep that invariant
    // here, and set instantiatedFrom for this class because its parent
    // was instantiated.
    if (parentClassType->instantiatedFrom() && !instantiatedFrom) {
      instantiatedFrom = initialTypeForTypeDecl(context, ad->id());
    }

    const BasicClassType* insnFromBct = nullptr;
    if (instantiatedFrom != nullptr) {
      if (auto bct = instantiatedFrom->toBasicClassType()) {
        insnFromBct = bct;
      } else if (auto ct = instantiatedFrom->toClassType()) {
        // safe because it's checked for null later.
        insnFromBct = ct->basicClassType();
      }

      if (!insnFromBct) {
        CHPL_ASSERT(false && "unexpected instantiatedFrom type");
      }
    }

    ret = BasicClassType::get(context, c->id(), c->name(),
                              parentClassType,
                              insnFromBct, std::move(filteredSubs));

  } else if (auto r = ad->toRecord()) {
    if (r->id() == DomainType::getGenericDomainType(context)->id()) {
      ret = DomainType::getGenericDomainType(context);
    } else if (r->id() == ArrayType::getGenericArrayType(context)->id()) {
      ret = ArrayType::getGenericArrayType(context);
    } else if (r->id() == CompositeType::getLocaleType(context)->id()) {
      ret = CompositeType::getLocaleType(context);
    } else {
      const RecordType* insnFromRec = nullptr;
      if (instantiatedFrom != nullptr) {
        if (auto rec = instantiatedFrom->toRecordType())
          insnFromRec = rec;
        else
          CHPL_ASSERT(false && "unexpected instantiatedFrom type");
      }

      ret = RecordType::get(context, r->id(), r->name(),
                            insnFromRec, std::move(filteredSubs));
    }

  } else if (auto u = ad->toUnion()) {
    const UnionType* insnFromUni = nullptr;
    if (instantiatedFrom != nullptr) {
      if (auto uni = instantiatedFrom->toUnionType())
        insnFromUni = uni;
      else
        CHPL_ASSERT(false && "unexpected instantiatedFrom type");
    }

    ret = UnionType::get(context, u->id(), u->name(),
                         insnFromUni, std::move(filteredSubs));

  } else {
    CHPL_ASSERT(false && "case not handled");
  }

  return ret;
}

// TODO:
// This code will be duplicating a lot of stuff in VarScopeVisitor, but it's
// different enough that I don't know how to proceed. I'm certain that there's
// a general way to make all these traversals work.

struct ReturnInferenceFrame;
struct ReturnInferenceSubFrame {
  // The AST node whose frame should be saved into this sub-frame
  const AstNode* astNode = nullptr;
  // The frame associated with the given AST node.
  owned<ReturnInferenceFrame> frame = nullptr;
  // Whether this sub-frame should be skipped when combining sub-results.
  // Occurs in particular when a branch is known statically not to occur.
  bool skip = false;

  ReturnInferenceSubFrame(const AstNode* node) : astNode(node) {}
};

struct ReturnInferenceFrame {
  const AstNode* scopeAst = nullptr;
  bool returnsOrThrows = false;
  bool breaks = false;
  bool continues = false;
  std::vector<ReturnInferenceSubFrame> subFrames;

  ReturnInferenceFrame(const AstNode* node) : scopeAst(node) {}
};

struct ReturnTypeInferrer {
  using RV = ResolvedVisitor<ReturnTypeInferrer>;

  // input
  ResolutionContext* rc;
  Context* context = rc ? rc->context() : nullptr;
  const Function* fnAst;
  Function::ReturnIntent returnIntent;
  Function::Kind functionKind;
  const Type* declaredReturnType;

  // intermediate information
  std::vector<owned<ReturnInferenceFrame>> returnFrames;

  // output
  std::vector<QualifiedType> returnedTypes;

  ReturnTypeInferrer(ResolutionContext* rc,
                     const Function* fn,
                     const Type* declaredReturnType)
    : rc(rc),
      fnAst(fn),
      returnIntent(fn->returnIntent()),
      functionKind(fn->kind()),
      declaredReturnType(declaredReturnType) {
  }

  void process(const uast::AstNode* symbol,
               ResolutionResultByPostorderID& byPostorder);

  // emits errors for invalid return type and adjusts it for magic special cases
  void checkReturn(const AstNode* inExpr, QualifiedType& qt);
  void noteVoidReturnType(const AstNode* inExpr);
  void noteReturnType(const AstNode* expr, const AstNode* inExpr, RV& rv);

  QualifiedType returnedType();

  ReturnInferenceSubFrame& currentThenFrame();
  ReturnInferenceSubFrame& currentElseFrame();

  void enterScope(const uast::AstNode* node);
  void exitScope(const uast::AstNode* node);

  bool markReturnOrThrow();
  bool hasReturnedOrThrown();

  bool hasHitBreak();
  bool hasHitBreakOrContinue();

  bool enter(const Function* fn, RV& rv);
  void exit(const Function* fn, RV& rv);

  bool enter(const Conditional* cond, RV& rv);
  void exit(const Conditional* cond, RV& rv);

  bool enter(const Select* sel, RV& rv);
  void exit(const Select* sel, RV& rv);

  bool enter(const For* forLoop, RV& rv);
  void exit(const For* forLoop, RV& rv);

  bool enter(const Break* brk, RV& rv);
  void exit(const Break* brk, RV& rv);

  bool enter(const Continue* cont, RV& rv);
  void exit(const Continue* cont, RV& rv);

  bool enter(const Return* ret, RV& rv);
  void exit(const Return* ret, RV& rv);

  bool enter(const Yield* ret, RV& rv);
  void exit(const Yield* ret, RV& rv);

  bool enter(const AstNode* ast, RV& rv);
  void exit(const AstNode* ast, RV& rv);
};

void ReturnTypeInferrer::process(const uast::AstNode* symbol,
                                 ResolutionResultByPostorderID& byPostorder) {
  ResolvedVisitor<ReturnTypeInferrer> rv(rc, symbol, *this, byPostorder);
  symbol->traverse(rv);
}

void ReturnTypeInferrer::checkReturn(const AstNode* inExpr,
                                     QualifiedType& qt) {
  if (!qt.type()) {
    return;
  }
  if (qt.type()->isVoidType()) {
    if (returnIntent == Function::REF) {
      context->error(inExpr, "Cannot return void with ref return intent");
    } else if (returnIntent == Function::PARAM) {
      context->error(inExpr, "Cannot return void with param return intent");
    } else if (returnIntent == Function::TYPE) {
      context->error(inExpr, "Cannot return void with type return intent");
    }
  } else {
    bool ok = true;
    if ((qt.isType() || qt.isParam()) &&
        (returnIntent == Function::CONST_REF ||
         returnIntent == Function::REF)) {
      ok = false;
    } else if (returnIntent == Function::TYPE && !qt.isType()) {
      ok = false;

      // runtime type builders have a body that returns a value, but
      // their intent is 'type'. Allow that.
      if (auto ag = fnAst->attributeGroup()) {
        if (ag->hasPragma(uast::pragmatags::PRAGMA_RUNTIME_TYPE_INIT_FN)) {
          ok = true;
          qt = QualifiedType(QualifiedType::TYPE, qt.type());
        }
      }
    } else if (returnIntent == Function::PARAM && !qt.isParam()) {
      ok = false;
    }
    if (!ok) {
      context->error(inExpr, "cannot return it with provided return intent");
    }
  }
}

void ReturnTypeInferrer::noteVoidReturnType(const AstNode* inExpr) {
  auto voidType = QualifiedType(QualifiedType::CONST_VAR, VoidType::get(context));
  returnedTypes.push_back(voidType);

  checkReturn(inExpr, voidType);
}
void ReturnTypeInferrer::noteReturnType(const AstNode* expr,
                                        const AstNode* inExpr,
                                        RV& rv) {
  QualifiedType qt = rv.byAst(expr).type();

  QualifiedType::Kind kind = qt.kind();
  const Type* type = qt.type();

  // Functions that return tuples need to return
  // a value tuple (for value returns and type returns)
  // or a reference to a value tuple (for ref/const ref returns)
  if (type && type->isTupleType()) {
    auto tt = type->toTupleType();
    type = tt->toValueTuple(context);
    qt = QualifiedType(kind, type);
  }

  checkReturn(inExpr, qt);
  returnedTypes.push_back(std::move(qt));
}

QualifiedType ReturnTypeInferrer::returnedType() {
  if (returnedTypes.size() == 0) {
    return QualifiedType(QualifiedType::CONST_VAR, VoidType::get(context));
  } else {
    auto retType = commonType(context, returnedTypes,
                              (QualifiedType::Kind) returnIntent);
    if (!retType) {
      // Couldn't find common type, so return type is incorrect.
      // TODO: replace with custom error class, and give more information about
      // why we couldn't determine a return type
      context->error(fnAst, "could not determine return type for function");
      retType = QualifiedType(QualifiedType::UNKNOWN, ErroneousType::get(context));
    }
    auto adjType = adjustForReturnIntent(returnIntent, *retType);
    return adjType;
  }
}

ReturnInferenceSubFrame& ReturnTypeInferrer::currentThenFrame() {
  CHPL_ASSERT(returnFrames.size() > 0);
  auto& topFrame = returnFrames.back();
  CHPL_ASSERT(topFrame->scopeAst->isConditional());
  return topFrame->subFrames[0];
}
ReturnInferenceSubFrame& ReturnTypeInferrer::currentElseFrame() {
  CHPL_ASSERT(returnFrames.size() > 0);
  auto& topFrame = returnFrames.back();
  CHPL_ASSERT(topFrame->scopeAst->isConditional());
  return topFrame->subFrames[1];
}

void ReturnTypeInferrer::enterScope(const uast::AstNode* node) {
  if (!createsScope(node->tag())) return;

  returnFrames.push_back(toOwned(new ReturnInferenceFrame(node)));
  auto& newFrame = returnFrames.back();

  if (returnFrames.size() > 1) {
    // If we returned in the parent frame, we already returned here.
    newFrame->returnsOrThrows |= returnFrames[returnFrames.size() - 2]->returnsOrThrows;
  }

  if (auto condNode = node->toConditional()) {
    newFrame->subFrames.emplace_back(condNode->thenBlock());
    newFrame->subFrames.emplace_back(condNode->elseBlock());
  } else if (auto selNode = node->toSelect()) {
    bool hasOtherwise = false;
    for (auto when : selNode->whenStmts()) {
      if (when->isOtherwise()) hasOtherwise = true;
      newFrame->subFrames.emplace_back(when);
    }
    if (!hasOtherwise) {
      // If an 'otherwise' case is not present, then we treat it as a
      // non-returning 'branch'.
      newFrame->subFrames.emplace_back(nullptr);
    }
  } else if (auto tryNode = node->toTry()) {
    newFrame->subFrames.emplace_back(tryNode->body());
    for (auto clause : tryNode->handlers()) {
      newFrame->subFrames.emplace_back(clause);
    }
  }
}

void ReturnTypeInferrer::exitScope(const uast::AstNode* node) {
  if (!createsScope(node->tag())) return;

  CHPL_ASSERT(returnFrames.size() > 0);
  auto poppingFrame = std::move(returnFrames.back());
  CHPL_ASSERT(poppingFrame->scopeAst == node);
  returnFrames.pop_back();

  bool parentReturnsOrThrows = poppingFrame->returnsOrThrows;
  bool parentBreaks = poppingFrame->breaks;
  bool parentContinues = poppingFrame->continues;

  if (poppingFrame->scopeAst->isLoop()) {
    if (!(poppingFrame->scopeAst->isFor() &&
          poppingFrame->scopeAst->toFor()->isParam())) {
      // Do not propagate info from non-param loops, as we can't statically
      // know what they'll do at runtime.
      parentReturnsOrThrows = false;
      parentBreaks = false;
      parentContinues = false;
    }
  }

  // Integrate sub-frame information.
  if (poppingFrame->subFrames.size() > 0) {
    bool allReturnOrThrow = true;
    bool allBreak = true;
    bool allContinue = true;
    bool allSkip = true;
    for (auto& subFrame : poppingFrame->subFrames) {
      if (subFrame.skip) {
        continue;
      } else {
        allSkip = false;
      }

      bool frameNonEmpty = subFrame.frame != nullptr;

      allReturnOrThrow &= frameNonEmpty && subFrame.frame->returnsOrThrows;
      allBreak &= frameNonEmpty && subFrame.frame->breaks;
      allContinue &= frameNonEmpty && subFrame.frame->continues;
    }

    // If all subframes skipped, then there were no returns.
    parentReturnsOrThrows = !allSkip && allReturnOrThrow;
    parentBreaks = !allSkip && allBreak;
    parentContinues = !allSkip && allContinue;
  }

  if (returnFrames.size() > 0) {
    // Might we become a sub-frame in another frame?
    auto& parentFrame = returnFrames.back();
    bool storedAsSubFrame = false;

    for (auto& subFrame : parentFrame->subFrames) {
      if (subFrame.astNode == node) {
        CHPL_ASSERT(
            !storedAsSubFrame &&
            "should not be possible to store a frame as multiple sub-frames");
        subFrame.frame = std::move(poppingFrame);
        storedAsSubFrame = true;
      }
    }

    if (!storedAsSubFrame) {
      parentFrame->returnsOrThrows |= parentReturnsOrThrows;
      if (!poppingFrame->scopeAst->isLoop()) {
        // propagate break/continue only up to the enclosing loop
        parentFrame->breaks |= parentBreaks;
        parentFrame->continues |= parentContinues;
      }
    }
  }
}

bool ReturnTypeInferrer::markReturnOrThrow() {
  if (returnFrames.empty()) return false;
  auto& topFrame = returnFrames.back();
  bool oldValue = topFrame->returnsOrThrows;
  topFrame->returnsOrThrows = true;
  return oldValue;
}

bool ReturnTypeInferrer::hasReturnedOrThrown() {
  if (returnFrames.empty()) return false;
  return returnFrames.back()->returnsOrThrows;
}

bool ReturnTypeInferrer::hasHitBreak() {
  if (returnFrames.empty()) return false;
  auto& topFrame = returnFrames.back();
  return topFrame->breaks;
}

bool ReturnTypeInferrer::hasHitBreakOrContinue() {
  if (returnFrames.empty()) return false;
  auto& topFrame = returnFrames.back();
  return topFrame->breaks || topFrame->continues;
}

bool ReturnTypeInferrer::enter(const Function* fn, RV& rv) {
  return false;
}
void ReturnTypeInferrer::exit(const Function* fn, RV& rv) {
}


bool ReturnTypeInferrer::enter(const Conditional* cond, RV& rv) {
  enterScope(cond);
  auto condition = cond->condition();
  CHPL_ASSERT(condition != nullptr);
  const ResolvedExpression& r = rv.byAst(condition);
  if (r.type().isParamTrue()) {
    auto then = cond->thenBlock();
    CHPL_ASSERT(then != nullptr);
    then->traverse(rv);
    // It doesn't matter if we don't return in the else frame, since it's
    // compiled out.
    currentElseFrame().skip = true;
    return false;
  } else if (r.type().isParamFalse()) {
    auto else_ = cond->elseBlock();
    if (else_) {
      else_->traverse(rv);
    }
    // It doesn't matter if we don't return in the then frame, since it's
    // compiled out.
    currentThenFrame().skip = true;
    return false;
  }
  return true;
}
void ReturnTypeInferrer::exit(const Conditional* cond, RV& rv) {
  exitScope(cond);
}

bool ReturnTypeInferrer::enter(const Select* sel, RV& rv) {
  enterScope(sel);


  int paramTrueIdx = -1;
  for (int i = 0; i < sel->numWhenStmts(); i++) {
    auto when = sel->whenStmt(i);

    bool anyParamTrue = false;
    bool allParamFalse = !when->isOtherwise(); //do not skip otherwise blocks

    for (auto caseExpr : when->caseExprs()) {
      auto res = rv.byAst(caseExpr);

      anyParamTrue = anyParamTrue || res.type().isParamTrue();
      allParamFalse = allParamFalse && res.type().isParamFalse();
    }

    //if all cases are param false, the path will never be visited.
    if (allParamFalse) {
      auto& topFrame = returnFrames.back();
      topFrame->subFrames[i].skip = true;
      continue;
    }

    when->traverse(rv);

    //if any case is param true, none of the following whens will be visited
    if (anyParamTrue) {
      paramTrueIdx = i;
      break;
    }
  }

  // if we found a param true case, mark the frames for the remaining whens
  if (paramTrueIdx == -1) return false;

  auto& topFrame = returnFrames.back();
  for (size_t i = paramTrueIdx+1; i < topFrame->subFrames.size(); i++) {
    topFrame->subFrames[i].skip = true;
  }

  return false;
}
void ReturnTypeInferrer::exit(const Select* sel, RV& rv) {
  exitScope(sel);
}

bool ReturnTypeInferrer::enter(const For* forLoop, RV& rv) {
  enterScope(forLoop);

  if (forLoop->isParam()) {
    // For param loops, "unroll" by manually traversing each iteration.
    const ResolvedExpression& rr = rv.byAst(forLoop);
    const ResolvedParamLoop* resolvedLoop = rr.paramLoop();
    CHPL_ASSERT(resolvedLoop);

    for (auto loopBody : resolvedLoop->loopBodies()) {
      RV loopVis(rv.rc(), forLoop, rv.userVisitor(), loopBody);
      for (const AstNode* child : forLoop->children()) {
        child->traverse(loopVis);
      }

      if (hasHitBreak() || hasReturnedOrThrown()) {
        // Stop processing subsequent loop bodies after a return or break
        // statement; they're never reached.
        break;
      }
    }

    return false;
  } else {
    return true;
  }
}
void ReturnTypeInferrer::exit(const For* forLoop, RV& rv) {
  exitScope(forLoop);
}

bool ReturnTypeInferrer::enter(const Break* brk, RV& rv) {
  CHPL_ASSERT(!returnFrames.empty());
  returnFrames.back()->breaks = true;
  return false;
}
void ReturnTypeInferrer::exit(const Break* brk, RV& rv) {}
bool ReturnTypeInferrer::enter(const Continue* cont, RV& rv) {
  CHPL_ASSERT(!returnFrames.empty());
  returnFrames.back()->continues = true;
  return false;
}
void ReturnTypeInferrer::exit(const Continue* cont, RV& rv) {}

bool ReturnTypeInferrer::enter(const Return* ret, RV& rv) {
  // Ignore subsequent returns after a break or continue statement.
  if (hasHitBreakOrContinue()) return false;

  if (markReturnOrThrow()) {
    // If it's statically known that we've already encountered a return
    // we can safely ignore subsequent returns.
    return false;
  }

  if (functionKind == Function::ITER) {
    // Plain returns don't count towards type inference for iterators.
    return false;
  }

  if (const AstNode* expr = ret->value()) {
    noteReturnType(expr, ret, rv);
  } else {
    noteVoidReturnType(ret);
  }
  return false;
}
void ReturnTypeInferrer::exit(const Return* ret, RV& rv) {
}

bool ReturnTypeInferrer::enter(const Yield* ret, RV& rv) {
  if (hasReturnedOrThrown()) {
    // If it's statically known that we've already encountered a return
    // we can safely ignore subsequent yields.
    return false;
  }

  noteReturnType(ret->value(), ret, rv);
  return false;
}
void ReturnTypeInferrer::exit(const Yield* ret, RV& rv) {
}

bool ReturnTypeInferrer::enter(const AstNode* ast, RV& rv) {
  enterScope(ast);
  return true;
}
void ReturnTypeInferrer::exit(const AstNode* ast, RV& rv) {
  exitScope(ast);
}

// For a class type construction, returns a BasicClassType
static const Type* const&
returnTypeForTypeCtorQuery(Context* context,
                           const TypedFnSignature* sig,
                           const PoiScope* poiScope) {
  QUERY_BEGIN(returnTypeForTypeCtorQuery, context, sig, poiScope);

  const UntypedFnSignature* untyped = sig->untyped();

  const Type* result = nullptr;

  // handle type construction
  const AggregateDecl* ad = nullptr;
  const Interface* itf = nullptr;
  if (!untyped->id().isEmpty()) {
    if (auto ast = parsing::idToAst(context, untyped->compilerGeneratedOrigin())) {
      ad = ast->toAggregateDecl();
      itf = ast->toInterface();
    }
  }

  if (ad) {
    // compute instantiatedFrom
    const CompositeType* instantiatedFrom = nullptr;
    if (sig->instantiatedFrom()) {
      const Type* t = initialTypeForTypeDecl(context, ad->id());
      CHPL_ASSERT(t);

      // ignore decorators etc for finding instantiatedFrom
      if (auto ct = t->toClassType()) {
        t = ct->basicClassType();
        CHPL_ASSERT(t && "Expecting initial type for type declaration to be concrete");
      }

      instantiatedFrom = t->toCompositeType();
      CHPL_ASSERT(instantiatedFrom);
    }

    // compute the substitutions
    SubstitutionsMap subs;

    if (instantiatedFrom != nullptr) {
      int nFormals = sig->numFormals();
      for (int i = 0; i < nFormals; i++) {
        auto field = findFieldByName(context, ad, instantiatedFrom, untyped->formalName(i));
        const QualifiedType& formalType = sig->formalType(i);
        // Note that the formalDecl should already be a fieldDecl
        // based on typeConstructorInitialQuery.
        auto useKind = formalType.kind();
        bool hasInitExpression = false;
        if (auto vd = field->toVarLikeDecl()) {
          // Substitute with the kind of the underlying field corresponding to
          // the formal. For example, if we substitute in a type for a generic
          // VAR decl, the type we construct will need to be inited with a VAR
          // of that corresponding type.
          useKind = vd->storageKind();
          if (vd->initExpression() != nullptr) {
            hasInitExpression = true;
          }
        }

        if (formalType.type()->isAnyType() && !hasInitExpression) {
          // Ignore this substitution - easier to just leave it out
          // of the map entirely.
          // Note that we explicitly put a sub for AnyType for generics
          // with default, where the default is not used. E.g.
          //    record R { type t = int; }
          //    type RR = R(?);
          //    var x: RR;
          // is a compilation error because x has generic type.
          // In order to support that pattern, we need to be able to
          // represent that RR is a version of R where it's not behaving
          // as generic-with-default and substituting in AnyType does that.
        } else {
          auto useQt =
              QualifiedType(useKind, formalType.type(), formalType.param());
          subs.insert({field->id(), useQt});
        }
      }
    }

    // get a type using the substitutions
    const CompositeType* theType = helpGetTypeForDecl(context, ad,
                                                      subs,
                                                      poiScope,
                                                      instantiatedFrom);

    result = theType;

  } else if (itf) {
    SubstitutionsMap subs;

    CHPL_ASSERT(sig->numFormals() == itf->numFormals());

    int nFormals = sig->numFormals();
    for (int i = 0; i < nFormals; i++) {
      auto& formalType = sig->formalType(i);
      auto& formalId = itf->formal(i)->id();
      subs.emplace(formalId, formalType);
    }

    result = InterfaceType::get(context, itf->id(), itf->name(), std::move(subs));
  } else {
    // built-in type construction should be handled
    // by resolveFnCallSpecialType and not reach this point.
    CHPL_ASSERT(false && "case not handled");
  }

  return QUERY_END(result);
}

static QualifiedType computeTypeOfField(ResolutionContext* rc,
                                        const Type* t,
                                        ID fieldId) {
  auto context = rc->context();
  if (const CompositeType* ct = t->getCompositeType()) {
    // Figure out the parent MultiDecl / TupleDecl
    ID declId = parsing::idToContainingMultiDeclId(context, fieldId);

    // Resolve the type of that field (or MultiDecl/TupleDecl)
    const auto& fields = resolveFieldDecl(rc, ct, declId,
                                          DefaultsPolicy::IGNORE_DEFAULTS);
    int n = fields.numFields();
    for (int i = 0; i < n; i++) {
      if (fields.fieldDeclId(i) == fieldId) {
        return fields.fieldType(i);
      }
    }
  }

  CHPL_ASSERT(false && "should not be reachable");
  return QualifiedType(QualifiedType::VAR, ErroneousType::get(context));
}

static QualifiedType adjustForReturnIntent(uast::Function::ReturnIntent ri,
                                           QualifiedType retType) {

  QualifiedType::Kind kind = (QualifiedType::Kind) ri;
  // adjust default / const return intent to 'var'
  if (kind == QualifiedType::DEFAULT_INTENT ||
      kind == QualifiedType::VAR) {
    kind = QualifiedType::CONST_VAR;
  }
  return QualifiedType(kind, retType.type(), retType.param());
}


struct CountReturns {
  // input
  Context* context;
  Function::Kind functionKind;

  // output
  int nReturnsWithValue = 0;
  int nReturnsWithoutValue = 0;
  const AstNode* firstWithValue = nullptr;
  const AstNode* firstWithoutValue = nullptr;

  CountReturns(Context* context, const Function* fn)
    : context(context), functionKind(fn->kind()) {
  }

  void countWithValue(const AstNode* ast);
  void countWithoutValue(const AstNode* ast);

  bool enter(const Function* fn);
  void exit(const Function* fn);

  bool enter(const Return* ret);
  void exit(const Return* ret);

  bool enter(const Yield* ret);
  void exit(const Yield* ret);

  bool enter(const AstNode* ast);
  void exit(const AstNode* ast);
};

void CountReturns::countWithValue(const AstNode* ast) {
  if (firstWithValue == nullptr) {
    firstWithValue = ast;
  }
  nReturnsWithValue++;
}

void CountReturns::countWithoutValue(const AstNode* ast) {
  if (firstWithoutValue == nullptr) {
    firstWithoutValue = ast;
  }
  nReturnsWithoutValue++;
}

bool CountReturns::enter(const Function* fn) {
  return false;
}
void CountReturns::exit(const Function* fn) {
}

bool CountReturns::enter(const Return* ret) {
  if (ret->value() != nullptr) {
    // Return statements don't contribute to the return type of iterators
    // (yields do). However, note it here anyway so that the return type
    // inference is kicked off, and that error reported there.
    countWithValue(ret);
  } else if (functionKind != Function::ITER) {
    // Only note void returns as returns if the function is not an iterator.
    // Iterators can return, but that just ends the iterator; the yields are
    // what provide the return type.
    countWithoutValue(ret);
  }
  return false;
}
void CountReturns::exit(const Return* ret) {
}

bool CountReturns::enter(const Yield* ret) {
  if (ret->value() != nullptr) {
    countWithValue(ret);
  } else {
    countWithoutValue(ret);
  }
  return false;
}
void CountReturns::exit(const Yield* ret) {
}

bool CountReturns::enter(const AstNode* ast) {
  return true;
}
void CountReturns::exit(const AstNode* ast) {
}

// vs. just returning 'void'
static const bool& fnAstReturnsNonVoid(Context* context, ID fnId) {
  QUERY_BEGIN(fnAstReturnsNonVoid, context, fnId);

  bool result = false;

  const AstNode* ast = parsing::idToAst(context, fnId);
  const Function* fn = ast->toFunction();
  CHPL_ASSERT(fn);

  CountReturns cr(context, fn);
  fn->body()->traverse(cr);

  result = (cr.nReturnsWithValue > 0);

  if (cr.nReturnsWithValue > 0 && cr.nReturnsWithoutValue > 0) {
    // TODO: make this a real error message
    context->error(cr.firstWithoutValue, "Mix of return types");
  }

  if (cr.nReturnsWithValue > 0 &&
      (fn->name() == USTR("init") || fn->name() == USTR("init="))) {
    context->error(cr.firstWithValue, "initializers can only return 'void'");
  }

  return QUERY_END(result);
}

static bool helpComputeOrderToEnumReturnType(Context* context,
                                             const TypedFnSignature* sig,
                                             QualifiedType& result) {
  auto firstQt = sig->formalType(0);
  auto secondQt = sig->formalType(1);

  CHPL_ASSERT(secondQt.type() && secondQt.type()->isEnumType());
  auto kind = QualifiedType::CONST_VAR;
  const EnumType* et = secondQt.type()->toEnumType();
  const Type* type = et;
  const Param* param = nullptr;

  if (firstQt.isParam()) {
    auto inputParam = firstQt.param();
    CHPL_ASSERT(inputParam);
    kind = QualifiedType::PARAM;

    // Use max value of int64 to represent the fact that the ordinal
    // is invalid (we will run out of enum elements to investigate before
    // we reach this value).
    uint64_t whichValue = std::numeric_limits<uint64_t>::max();
    if (auto intParam = inputParam->toIntParam()) {
      if (intParam->value() >= 0) {
        whichValue = intParam->value();
      }
    } else if (auto uintParam = inputParam->toUintParam()) {
      whichValue = uintParam->value();
    } else {
      CHPL_ASSERT(false && "param value should've been integral");
    }

    auto ast =
      parsing::idToAst(context, et->id())->toEnum();
    uint64_t counter = 0;
    for (auto elem : ast->enumElements()) {
      if (counter == whichValue) {
        param = Param::getEnumParam(context, elem->id());
        break;
      }
      counter++;
    }

    if (!param) {
      context->error(ast, "ordinal value out of range");
      type = ErroneousType::get(context);
    }
  }

  result = QualifiedType(kind, type, param);
  return true;
}

static bool helpComputeEnumToOrderReturnType(Context* context,
                                             const TypedFnSignature* sig,
                                             QualifiedType& result) {
  auto firstQt = sig->formalType(0);

  CHPL_ASSERT(firstQt.type() && firstQt.type()->isEnumType());
  auto kind = QualifiedType::CONST_VAR;
  const EnumType* et = firstQt.type()->toEnumType();
  const Type* type = IntType::get(context, 0);
  const Param* param = nullptr;

  if (firstQt.isParam()) {
    auto inputParam = firstQt.param()->toEnumParam();
    CHPL_ASSERT(inputParam);
    kind = QualifiedType::PARAM;

    auto ast =
      parsing::idToAst(context, et->id())->toEnum();
    int counter = 0;
    for (auto elem : ast->enumElements()) {
      if (elem->id() == inputParam->value().id) {
        param = IntParam::get(context, counter);
        break;
      }
      counter++;
    }

    CHPL_ASSERT(param);
  }

  result = QualifiedType(kind, type, param);
  return true;
}

static bool helpComputeCompilerGeneratedReturnType(ResolutionContext* rc,
                                                   const TypedFnSignature* sig,
                                                   const PoiScope* poiScope,
                                                   QualifiedType& result,
                                                   const UntypedFnSignature* untyped) {
  auto context = rc->context();
  if (untyped->name() == USTR("init") ||
      untyped->name() == USTR("init=") ||
      untyped->name() == USTR("deinit") ||
      untyped->name() == USTR("=") ||
      untyped->name() == USTR("serialize") ||
      untyped->name() == USTR("deserialize")) {
      result = QualifiedType(QualifiedType::CONST_VAR,
                             VoidType::get(context));
      return true;
  } else if (untyped->name() == USTR("==")) {
      result = QualifiedType(QualifiedType::CONST_VAR, BoolType::get(context));
      return true;
  } else if (untyped->name() == USTR(":")) {
    // Assume that compiler-generated casts are actually cast.
    auto input = sig->formalType(0);
    auto outputType = sig->formalType(1);

    result = QualifiedType(input.kind(), outputType.type());
    return true;
  } else if (untyped->isMethod() && sig->formalType(0).type()->isIteratorType() &&
             untyped->name() == "_shape_") {
    auto it = sig->formalType(0).type()->toIteratorType();
    auto shape = shapeForIterator(context, it);
    CHPL_ASSERT(shape);
    result = QualifiedType(QualifiedType::VAR, shape);
    return true;
  } else if (untyped->idIsField() && untyped->isMethod()) {
    // method accessor - compute the type of the field
    QualifiedType ft = computeTypeOfField(rc,
                                          sig->formalType(0).type(),
                                          untyped->id());
    if (ft.isType() || ft.isParam()) {
      // return the type as-is (preserving param/type-ness)
      result = ft;
    } else if (ft.isConst()) {
      // return a const ref
      result = QualifiedType(QualifiedType::CONST_REF, ft.type());
    } else {
      // return a ref
      result = QualifiedType(QualifiedType::REF, ft.type());
    }
    return true;
  } else if (untyped->isMethod() && sig->formalType(0).type()->isPtrType() &&
             untyped->name() == "eltType") {
    auto pt = sig->formalType(0).type()->toPtrType();
    result = QualifiedType(QualifiedType::TYPE, pt->eltType());
    return true;
  } else if (untyped->isMethod() &&
             sig->formalType(0).type()->isHeapBufferType() &&
             untyped->name() == "this") {
    auto pt = sig->formalType(0).type()->toHeapBufferType();
    result = QualifiedType(QualifiedType::REF, pt->eltType());
    return true;
  } else if (untyped->isMethod() && sig->formalType(0).type()->isEnumType()) {
    auto enumType = sig->formalType(0).type()->toEnumType();
    if (untyped->name() == "size") {
      auto ast = parsing::idToAst(context, enumType->id())->toEnum();
      CHPL_ASSERT(ast);
      int numElts = ast->numElements();
      result = QualifiedType::makeParamInt(context, numElts);
      return true;
    }
    CHPL_ASSERT(false && "unhandled compiler-generated enum method");
    return true;
  } else if (!untyped->isMethod()) {
    if (untyped->name() == "chpl__orderToEnum") {
      return helpComputeOrderToEnumReturnType(context, sig, result);
    } else if (untyped->name() == "chpl__enumToOrder") {
      return helpComputeEnumToOrderReturnType(context, sig, result);
    }
    CHPL_ASSERT(false && "unhandled compiler-generated function");
    return true;
  } else {
    CHPL_ASSERT(false && "unhandled compiler-generated record method");
    return true;
  }
}

// returns 'true' if it was a case handled here & sets 'result' in that case
// returns 'false' if it needs to be computed with a ResolvedVisitor traversal
static bool helpComputeReturnType(ResolutionContext* rc,
                                  const TypedFnSignature* sig,
                                  const PoiScope* poiScope,
                                  QualifiedType& result) {
  Context* context = rc->context();
  const UntypedFnSignature* untyped = sig->untyped();

  // TODO: Optimize the order of this case and the isCompilerGenerated case
  // such that we don't worry about instantiating the signature if we it's
  // compiler generated and one of the ops we know the return type for, e.g.
  // `==`, `init`, `init=`, `deinit`, and `=`.
  if (untyped->isTypeConstructor()) {
    const Type* t = returnTypeForTypeCtorQuery(context, sig, poiScope);

    // for a 'class C' declaration, the above query returns a BasicClassType,
    // but 'C' normally means a generic-management non-nil C
    // so adjust the result.
    if (auto bct = t->toBasicClassType()) {
      auto dec = ClassTypeDecorator(ClassTypeDecorator::GENERIC_NONNIL);
      t = ClassType::get(context, bct, /*manager*/ nullptr, dec);
    }

    result = QualifiedType(QualifiedType::TYPE, t);
    return true;

    // special case to set param value of tuple size, which is set late by
    // production compiler
  } else if (untyped->idIsFunction() && sig->needsInstantiation()) {
    // if it needs instantiation, we don't know the return type yet.
    result = QualifiedType(QualifiedType::UNKNOWN, UnknownType::get(context));
    return true;
  } else if (untyped->idIsFunction()) {
    const AstNode* ast = parsing::idToAst(context, untyped->id());
    const Function* fn = ast->toFunction();
    CHPL_ASSERT(fn);

    if (const AstNode* retType = fn->returnType()) {
      // Cannot return the correct "return type" for something value-less like
      // "param : int".
      if (fn->returnIntent() == Function::ReturnIntent::PARAM) {
        return false;
      }

      // resolve the return type
      ResolutionResultByPostorderID resolutionById;
      auto visitor = Resolver::createForFunction(rc, fn, poiScope, sig,
                                                 resolutionById);
      retType->traverse(visitor);
      result = resolutionById.byAst(retType).type();

      auto g = getTypeGenericity(context, result.type());
      if (g == Type::CONCRETE) {
        result = adjustForReturnIntent(fn->returnIntent(), result);
        return true;
      }
    }

    // if there are no returns with a value, use void return type
    if (fn->linkage() == Decl::EXTERN) {
      if (fn->returnType() == nullptr) {
        result = QualifiedType(QualifiedType::CONST_VAR, VoidType::get(context));
        return true;
      } else {
        // TODO: This is a workaround for a bug where the return type was
        // not found for some reason. By returning a type we prevent an attempt
        // to resolve the non-existent function body.
        result = QualifiedType(QualifiedType::CONST_VAR, UnknownType::get(context));
        return true;
      }
    } else if (fnAstReturnsNonVoid(context, ast->id()) == false) {
      result = QualifiedType(QualifiedType::CONST_VAR, VoidType::get(context));
      return true;
    }

    // otherwise, need to use visitor to get the return type
    return false;

  } else if (untyped->isMethod() && sig->formalType(0).type()->isTupleType() &&
             untyped->name() == "size") {
    auto tup = sig->formalType(0).type()->toTupleType();
    result = QualifiedType::makeParamInt(context, tup->numElements());
    return true;

    // if method call and the receiver points to a composite type definition,
    // then it's some sort of compiler-generated method
  } else if (untyped->isCompilerGenerated()) {
    return helpComputeCompilerGeneratedReturnType(rc, sig, poiScope,
                                                  result, untyped);
  } else {
    CHPL_ASSERT(false && "case not handled");
    return true;
  }

  return false;
}

static const QualifiedType&
returnTypeWithoutIterableQuery(ResolutionContext* rc,
                               const TypedFnSignature* sig,
                               const PoiScope* poiScope) {
  CHPL_RESOLUTION_QUERY_BEGIN(returnTypeWithoutIterableQuery, rc, sig, poiScope);

  Context* context = rc->context();
  const UntypedFnSignature* untyped = sig->untyped();
  QualifiedType result;

  bool computed = helpComputeReturnType(rc, sig, poiScope, result);
  if (!computed) {
    const AstNode* ast = parsing::idToAst(context, untyped->id());
    const Function* fn = ast->toFunction();
    CHPL_ASSERT(fn);

    // resolve the function body
    // resolveFunction will arrange to call computeReturnType
    // and store the return type in the result.
    if (auto rFn = resolveFunction(rc, sig, poiScope)) {
      result = rFn->returnType();
    }
  }

  return CHPL_RESOLUTION_QUERY_END(result);
}

static const QualifiedType& returnTypeQuery(ResolutionContext* rc,
                                            const TypedFnSignature* sig,
                                            const PoiScope* poiScope) {
  CHPL_RESOLUTION_QUERY_BEGIN(returnTypeQuery, rc, sig, poiScope);

  Context* context = rc->context();
  auto result = returnTypeWithoutIterableQuery(rc, sig, poiScope);

  if (sig->isIterator() && !result.isUnknownOrErroneous()) {
    result = QualifiedType(result.kind(),
                           FnIteratorType::get(context, poiScope, sig));

  }

  return CHPL_RESOLUTION_QUERY_END(result);
}

QualifiedType returnType(ResolutionContext* rc,
                         const TypedFnSignature* sig,
                         const PoiScope* poiScope) {
  return returnTypeQuery(rc, sig, poiScope);
}

QualifiedType yieldType(ResolutionContext* rc,
                        const TypedFnSignature* sig,
                        const PoiScope* poiScope) {
  CHPL_ASSERT(sig->isIterator());
  return returnTypeWithoutIterableQuery(rc, sig, poiScope);
}

static const TypedFnSignature* const&
inferOutFormalsQuery(ResolutionContext* rc,
                     const TypedFnSignature* sig,
                     const PoiScope* instantiationPoiScope) {
  CHPL_RESOLUTION_QUERY_BEGIN(inferOutFormalsQuery, rc, sig,
                              instantiationPoiScope);

  Context* context = rc->context();
  const UntypedFnSignature* untyped = sig->untyped();
  std::vector<types::QualifiedType> formalTypes;

  // resolve the function body
  if (auto rFn = resolveFunction(rc, sig, instantiationPoiScope)) {
    const ResolutionResultByPostorderID& rr = rFn->resolutionById();

    int numFormals = sig->numFormals();
    for (int i = 0; i < numFormals; i++) {
      const types::QualifiedType& ft = sig->formalType(i);
      if (ft.kind() == QualifiedType::OUT && ft.isGenericOrUnknown()) {
        formalTypes.push_back(rr.byAst(untyped->formalDecl(i)).type());
      } else {
        formalTypes.push_back(ft);
      }
    }
  }

  const TypedFnSignature* result = nullptr;
  result = TypedFnSignature::getInferred(context,
                                         std::move(formalTypes),
                                         sig);

  return CHPL_RESOLUTION_QUERY_END(result);
}

const TypedFnSignature* inferOutFormals(ResolutionContext* rc,
                                        const TypedFnSignature* sig,
                                        const PoiScope* instantiationPoiScope) {
  if (sig == nullptr) {
    return nullptr;
  }

  bool anyGenericOutFormals = false;
  int numFormals = sig->numFormals();
  for (int i = 0; i < numFormals; i++) {
    const types::QualifiedType& ft = sig->formalType(i);
    if (ft.kind() == QualifiedType::OUT && ft.isGenericOrUnknown()) {
      anyGenericOutFormals = true;
      break;
    }
  }

  // if there are no 'out' formals with generic type, just return 'sig'.
  // also just return 'sig' if the function needs instantiation;
  // in that case, we can't infer the 'out' formals by resolving the body.
  if (anyGenericOutFormals && !sig->needsInstantiation()) {
    return inferOutFormalsQuery(rc, sig, instantiationPoiScope);
  } else {
    return sig;
  }
}

void computeReturnType(Resolver& resolver) {
  QualifiedType returnType;
  bool computed = helpComputeReturnType(resolver.rc,
                                        resolver.typedSignature,
                                        resolver.poiScope,
                                        returnType);
  if (computed) {
    resolver.returnType = returnType;
  } else if (auto fn = resolver.symbol->toFunction()) {
    const Type* declaredReturnType = nullptr;
    if (auto retTypeExpr = fn->returnType()) {
      auto qt = resolver.byPostorder.byAst(retTypeExpr).type();
      declaredReturnType = qt.type();

      if (declaredReturnType && declaredReturnType->isUnknownType()) {
        declaredReturnType = nullptr;
      }
    }

    // infer the return type
    if (fn->linkage() != Decl::EXTERN) {
      auto v = ReturnTypeInferrer(resolver.rc, fn, declaredReturnType);
      v.process(fn->body(), resolver.byPostorder);
      resolver.returnType = v.returnedType();
    }
  }
}


} // end namespace resolution
} // end namespace chpl
