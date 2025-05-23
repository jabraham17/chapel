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

// checks.cpp

#include "checks.h"

#include "driver.h"
#include "expr.h"
#include "PartialCopyData.h"
#include "passes.h"
#include "primitive.h"
#include "resolution.h"
#include "TryStmt.h"


#include "global-ast-vecs.h"

//
// Static function declarations.
//
// (These collect checks applicable to various phases.)
//

static void check_afterEveryPass(); // Checks to be performed after every pass.
static void check_afterScopeResolve(); // Checks to be performed after the
                                       // scopeResolve pass.
static void check_afterNormalization(); // Checks to be performed after
                                        // normalization.
static void check_afterResolution(); // Checks to be performed after every pass
                                     // following resolution.
static void check_afterResolveIntents();
static void check_afterLowerErrorHandling();
static void check_afterCallDestructors(); // Checks to be performed after every
                                          // pass following callDestructors.
static void check_afterLowerIterators();
static void checkIsIterator(); // Ensure each iterator is flagged so.
static void check_afterInlineFunctions();
static void checkResolveRemovedPrims(void); // Checks that certain primitives
                                            // are removed after resolution
static void checkTaskRemovedPrims(); // Checks that certain primitives are
                                     // removed after task functions are
                                     // created.
static void checkLowerIteratorsRemovedPrims();
static void checkFlagRelationships(); // Checks expected relationships between
                                      // flags.
static void checkAutoCopyMap();
static void checkInitEqAssignCast();
static void checkFormalActualBaseTypesMatch();
static void checkRetTypeMatchesRetVarType();
static void checkFormalActualTypesMatch();


//
// Implementations.
//

void check_parseAndConvertUast()
{
  check_afterEveryPass();
}

void check_checkGeneratedAst()
{
  // checkIsIterator() will crash if there were certain USR_FATAL_CONT()
  // e.g. functions/vass/proc-iter/error-yield-in-proc-*
  exitIfFatalErrorsEncountered();
  checkIsIterator();
}

void check_readExternC()
{
  check_afterEveryPass();
  // Suggestion: Ensure extern C types defined.
}

void check_expandExternArrayCalls()
{
  check_afterEveryPass();
}

void check_cleanup()
{
  check_afterEveryPass();
  // Suggestion: Identify and check normalizations applied by cleanup pass.
}

void check_scopeResolve()
{
  check_afterEveryPass();
  check_afterScopeResolve();
  // Suggestion: Ensure identifiers resolved to scope.
}

void check_flattenClasses()
{
  check_afterEveryPass();
  // Suggestion: Ensure classes have no nested class definitions.
}

void check_normalize()
{
  check_afterEveryPass();
}

void check_checkNormalized()
{
  // The checkNormalized pass should not make any changes, so skip checks.
}

void check_buildDefaultFunctions()
{
  check_afterEveryPass();
  check_afterNormalization();
  // Suggestion: Ensure each class/record type has its own default functions.
}

void check_createTaskFunctions()
{
  checkTaskRemovedPrims();
  check_afterEveryPass();
  check_afterNormalization();
}

void check_resolve()
{
  checkResolveRemovedPrims();
  check_afterEveryPass();
  check_afterNormalization();
  checkReturnTypesHaveRefTypes();
  checkAutoCopyMap();
  checkInitEqAssignCast();
}

void check_resolveIntents()
{
  check_afterEveryPass();
  check_afterNormalization();
  check_afterResolution();
  check_afterResolveIntents();
  // Suggestion: Ensure now using a reduced set of intents.
}

void check_checkResolved()
{
  // The checkResolved pass should not make any changes, so skip checks.
}

void check_replaceArrayAccessesWithRefTemps()
{
  check_afterEveryPass();
  check_afterNormalization();
  check_afterResolution();
  check_afterResolveIntents();
}

void check_flattenFunctions()
{
  check_afterEveryPass();
  check_afterNormalization();
  check_afterResolution();
  check_afterResolveIntents();
  // Suggestion: Ensure no nested functions.
}

static
bool symbolIsUsedAsRef(Symbol* sym) {

  auto checkForMove = [](SymExpr* use, CallExpr* call) {
    SymExpr* lhs = toSymExpr(call->get(1));
    Symbol* lhsSymbol = lhs->symbol();
    return lhs != use && symbolIsUsedAsRef(lhsSymbol);
  };

  for_SymbolSymExprs(se, sym) {
    if (symExprIsUsedAsRef(se, false, checkForMove)) return true;
  }
  return false;
}

static
void checkForPromotionsThatMayRace() {
  // skip check if warning is off
  if (!fWarnPotentialRaces) return;

  // for all CallExprs, if we call a promotion wrapper that is marked no promotion, warn
  // checking here after all ContextCallExpr's have been resolved to plain CallExpr's
  for_alive_in_Vec(CallExpr, ce, gCallExprs) {

    if (FnSymbol* fn = ce->theFnSymbol()) {
      if (fn->hasFlag(FLAG_PROMOTION_WRAPPER) &&
          fn->hasFlag(FLAG_NO_PROMOTION_WHEN_BY_REF)) {

        // We cannot rely on retTag to tell us if this promoted function returns
        // a ref or not, we need to use the result of the call and see if it is
        // used in any ref contexts.
        // Assuming ce is used as a move/assign, get the lhs as a SymExpr. If its
        // symbol is used as a ref (either a ref var or passed to a ref formal)
        // then we should warn

        if (CallExpr* parentCe = toCallExpr(ce->parentExpr)) {
          if (isMoveOrAssign(parentCe)) {
            if (SymExpr* lhs = toSymExpr(parentCe->get(1))) {
              if(symbolIsUsedAsRef(lhs->symbol())) {
                USR_WARN(ce,
                         "modifying the result of a promoted index expression "
                         "is a potential race condition");
              }
            }
          }
        }
      }
    }
  }
}

void check_cullOverReferences()
{
  check_afterEveryPass();
  check_afterNormalization();
  check_afterResolution();
  check_afterResolveIntents();

  // No ContextCallExprs should remain in the tree.
  for_alive_in_Vec(ContextCallExpr, cc, gContextCallExprs) {
    INT_FATAL("ContextCallExpr should no longer be in AST");
  }

  checkForPromotionsThatMayRace();
}

void check_lowerErrorHandling()
{
  check_afterEveryPass();
  check_afterNormalization();
  check_afterLowerErrorHandling();
}

void check_callDestructors()
{
  check_afterEveryPass();
  check_afterNormalization();
  check_afterCallDestructors();
  // Suggestion: Ensure every constructor call has a matching destructor call.
}

void check_lowerIterators()
{
  check_afterEveryPass();
  check_afterNormalization();
  check_afterCallDestructors();
  check_afterLowerIterators();
  check_afterResolveIntents();
//  check_afterResolution(); // Oho! Iterator functions do not obey the invariant
  // checked in checkReturnPaths() [semanticChecks.cpp:250].
  // So check_afterResolution has been disabled in this and all subsequent post-pass
  // checks.
  // lowerIterators() should be revised to produce code that obeys all
  // invariants and then these (paranoid) tests re-enabled.
}

void check_parallel()
{
  check_afterEveryPass();
  check_afterNormalization();
  check_afterCallDestructors();
  check_afterLowerIterators();
  check_afterResolveIntents();
  // Suggestion: Ensure parallelization applied (if not --local).
}

void check_prune()
{
  check_afterEveryPass();
  check_afterNormalization();
  check_afterCallDestructors();
  check_afterLowerIterators();
  check_afterResolveIntents();
  // Suggestion: Ensure no dead classes or functions.
}

void check_bulkCopyRecords()
{
  check_afterEveryPass();
  check_afterNormalization();
  check_afterCallDestructors();
  check_afterLowerIterators();
  check_afterResolveIntents();
}

void check_removeUnnecessaryAutoCopyCalls()
{
  check_afterEveryPass();
  check_afterNormalization();
  check_afterCallDestructors();
  check_afterLowerIterators();
  check_afterResolveIntents();
  // Suggestion: Ensure no unnecessary autoCopy calls.
}

void check_inlineFunctions()
{
  check_afterEveryPass();
  check_afterNormalization();
  check_afterCallDestructors();
  check_afterLowerIterators();
  check_afterResolveIntents();
  check_afterInlineFunctions();
}

void check_scalarReplace()
{
  check_afterEveryPass();
  check_afterNormalization();
  check_afterCallDestructors();
  check_afterLowerIterators();
  check_afterResolveIntents();
  check_afterInlineFunctions();
  // Suggestion: Ensure no constant expressions.
}

void check_refPropagation()
{
  check_afterEveryPass();
  check_afterNormalization();
  check_afterCallDestructors();
  check_afterLowerIterators();
  check_afterResolveIntents();
  check_afterInlineFunctions();
}

void check_copyPropagation()
{
  check_afterEveryPass();
  check_afterNormalization();
  check_afterCallDestructors();
  check_afterLowerIterators();
  check_afterResolveIntents();
  check_afterInlineFunctions();
}


void check_deadCodeElimination()
{
  check_afterEveryPass();
  check_afterNormalization();
  check_afterCallDestructors();
  check_afterLowerIterators();
  check_afterResolveIntents();
  check_afterInlineFunctions();
  // Suggestion: Ensure no dead code.
}

void check_removeEmptyRecords()
{
  check_afterEveryPass();
  check_afterNormalization();
  check_afterCallDestructors();
  check_afterLowerIterators();
  check_afterResolveIntents();
  check_afterInlineFunctions();
  // Suggestion: Ensure no empty records.
}

void check_localizeGlobals()
{
  check_afterEveryPass();
  check_afterNormalization();
  check_afterCallDestructors();
  check_afterLowerIterators();
  check_afterResolveIntents();
  check_afterInlineFunctions();
}

void check_loopInvariantCodeMotion()
{
  check_afterEveryPass();
  check_afterNormalization();
  check_afterCallDestructors();
  check_afterLowerIterators();
  check_afterResolveIntents();
  check_afterInlineFunctions();
}

void check_prune2()
{
  check_afterEveryPass();
  check_afterNormalization();
  check_afterCallDestructors();
  check_afterLowerIterators();
  check_afterResolveIntents();
  check_afterInlineFunctions();
  // Suggestion: Ensure no dead classes or functions.
}

void check_returnStarTuplesByRefArgs()
{
  check_afterEveryPass();
  check_afterNormalization();
  check_afterCallDestructors();
  check_afterLowerIterators();
  check_afterResolveIntents();
  check_afterInlineFunctions();
}

void check_insertWideReferences()
{
  check_afterEveryPass();
  check_afterNormalization();
  check_afterCallDestructors();
  check_afterLowerIterators();
  check_afterResolveIntents();
  check_afterInlineFunctions();
}

void check_optimizeOnClauses()
{
  check_afterEveryPass();
  check_afterNormalization();
  check_afterCallDestructors();
  check_afterLowerIterators();
  check_afterResolveIntents();
  check_afterInlineFunctions();
}

void check_addInitCalls()
{
  check_afterEveryPass();
  check_afterNormalization();
  check_afterCallDestructors();
  check_afterLowerIterators();
  check_afterResolveIntents();
  check_afterInlineFunctions();
}

void check_insertLineNumbers()
{
  check_afterEveryPass();
  check_afterNormalization();
  check_afterCallDestructors();
  check_afterLowerIterators();
  check_afterInlineFunctions();
}

void check_denormalize() {
  //do we need to call any checks here ?
  //or implement new checks ?
}

void check_codegen()
{
  // This pass should not change the AST, so no checks are required.
}

void check_makeBinary()
{
  // This pass should not change the AST, so no checks are required.
}

//////////////////////////////////////////////////////////////////////////
// Utility functions
//

// Extra structural checks on the AST, applicable to all passes.
void check_afterEveryPass()
{
  if (fVerify)
  {
    verify();
    checkForDuplicateUses();
    checkFlagRelationships();
    checkEmptyPartialCopyDataFnMap();
  }
}

static void check_afterScopeResolve()
{
  // Check that 'out' arguments are not referred to by later arguments.
  // An alternative to this code would be to make scopeResolve not find
  // them at all when processing the other formals.
  for_alive_in_Vec(FnSymbol, fn, gFnSymbols) {
    // step 1: skip check if there are no out formals
    bool anyOutFormals = false;
    for_formals(formal, fn) {
      if (formal->intent == INTENT_OUT) {
        anyOutFormals = true;
        break;
      }
    }

    if (anyOutFormals) {
      // step 2: check each formal for a reference to
      // a previous out formal.
      for_formals(formal, fn) {

        if (formal->intent == INTENT_OUT && formal->variableExpr != NULL)
          USR_FATAL_CONT(formal, "out intent varargs are not supported");

        for_formals(earlierFormal, fn) {
          // stop if we get to the same formal as the above loop
          if (earlierFormal == formal)
            break;
          if (earlierFormal->intent == INTENT_OUT) {
            // check that earlierFormal is not used in formal's defExpr
            if (findSymExprFor(formal->defPoint, earlierFormal))
              USR_FATAL_CONT(formal,
                             "out-intent formal '%s' is used within "
                             "later formal '%s'",
                             earlierFormal->name, formal->name);
          }
        }
      }
    }
  }
}

// Checks that should remain true after the normalization pass is complete.
static void check_afterNormalization()
{
  if (fVerify)
  {
    checkNormalized();
  }
}

// Checks that should remain true after the functionResolution pass is complete.
static void check_afterResolution()
{
  checkReturnTypesHaveRefTypes();
  if (fVerify)
  {
    checkTaskRemovedPrims();
    checkResolveRemovedPrims();
// Disabled for now because user warnings should not be logged multiple times:
//    checkResolved();
    checkFormalActualBaseTypesMatch();
    checkRetTypeMatchesRetVarType();
    checkAutoCopyMap();
    checkInitEqAssignCast();
  }
}

static void check_afterResolveIntents()
{
  if (fVerify) {
    for_alive_in_Vec(DefExpr, def, gDefExprs) {
      Symbol* sym = def->sym;
      // Only look at Var or Arg symbols
      if (isLcnSymbol(sym)) {
        QualifiedType qual = sym->qualType();
        // MPF TODO: This should not be necessary
        // it is a workaround for problems with --verify
        // with tuple type constructors accepting domains.
        // It would be better to treat run-time types as
        // normal records.
        if (ArgSymbol* arg = toArgSymbol(sym))
          if (arg->intent == INTENT_TYPE)
            continue;

        if (qual.getQual() == QUAL_UNKNOWN) {
          INT_FATAL("Symbol should not have unknown qualifier: %s (%d)", sym->cname, sym->id);
        }
      }
    }
  }
}


static void check_afterLowerErrorHandling()
{
  if (fVerify)
  {
    // check that TryStmt is not in the tree
    forv_Vec(TryStmt, stmt, gTryStmts)
    {
      if (stmt->inTree())
        INT_FATAL(stmt, "TryStmt should no longer be in the tree");
    }

    // TODO: check no more CatchStmt

    // check no more PRIM_THROW
    forv_Vec(CallExpr, call, gCallExprs) {
      if (call->isPrimitive(PRIM_THROW) && call->inTree()) {
        INT_FATAL(call, "PRIM_THROW should no longer exist");
      }

      auto ft = call->isIndirectCall() ? call->functionType() : nullptr;
      if (ft && ft->throws()) {
        INT_FATAL(call, "Indirect calls to throwing functions should not "
                        "appear after this point!");
      }
    }
  }
}


// Checks that should remain true after the callDestructors pass is complete.
static void check_afterCallDestructors()
{
  // Disabled because it is not true after the first prune pass.
  //  checkReturnTypesHaveRefTypes();
  if (fVerify)
  {
// Disabled for now because user warnings should not be logged multiple times:
//    checkResolved();
    checkFormalActualTypesMatch();
  }
}


static void check_afterLowerIterators()
{
  checkLowerIteratorsRemovedPrims();
  if (fVerify)
    checkArgsAndLocals();
}

static void check_afterInlineFunctions() {
  if (fVerify) {
    forv_Vec(DefExpr, def, gDefExprs) {
      Symbol* sym = def->sym;
      if (isLcnSymbol(sym) &&
          def->inTree() && // symbol is in the tree
          def->parentSymbol->hasFlag(FLAG_WIDE_REF) == false) {
        if (sym->type->symbol->hasFlag(FLAG_REF) ||
            sym->type->symbol->hasFlag(FLAG_WIDE_REF)) {
         // "_interim" args added in gpuTransforms.cpp have ref types
         if (! def->parentSymbol->hasFlag(FLAG_GPU_CODEGEN))
          INT_FATAL("Found reference type: %s[%d]\n", sym->cname, sym->id);
        }
      }
    }
  }
}

static void checkIsIterator() {
  forv_Vec(CallExpr, call, gCallExprs) {
    if (call->isPrimitive(PRIM_YIELD)) {
      FnSymbol* fn = toFnSymbol(call->parentSymbol);
      // Violations should have caused USR_FATAL_CONT in checkGeneratedAst().
      INT_ASSERT(fn && fn->isIterator());
    }
  }
}


//
// Checks that certain primitives are removed after function resolution
//
static void
checkResolveRemovedPrims(void) {
  for_alive_in_Vec(CallExpr, call, gCallExprs) {
    if (call->primitive) {
      switch(call->primitive->tag) {
        case PRIM_BLOCK_PARAM_LOOP:

        case PRIM_DEFAULT_INIT_VAR:
        case PRIM_INIT_FIELD:
        case PRIM_INIT_VAR:

        case PRIM_LOGICAL_FOLDER:
        case PRIM_TYPEOF:
        case PRIM_TYPE_TO_STRING:
        case PRIM_IS_TUPLE_TYPE:
        case PRIM_IS_STAR_TUPLE_TYPE:
        case PRIM_IS_SUBTYPE:
        case PRIM_REDUCE:
        case PRIM_REDUCE_ASSIGN:
        case PRIM_TUPLE_EXPAND:
        case PRIM_QUERY:
        case PRIM_QUERY_PARAM_FIELD:
        case PRIM_QUERY_TYPE_FIELD:
        case PRIM_ERROR:
        case PRIM_COERCE:
        case PRIM_GATHER_TESTS:
          if (call->parentSymbol)
            INT_FATAL("Primitive should no longer be in AST");
          break;
        default:
          break;
      }
    }
  }
}

static void
checkTaskRemovedPrims()
{
  for_alive_in_Vec(CallExpr, call, gCallExprs)
    if (call->primitive)
      switch(call->primitive->tag)
      {
       case PRIM_BLOCK_BEGIN:
       case PRIM_BLOCK_COBEGIN:
       case PRIM_BLOCK_COFORALL:
       case PRIM_BLOCK_ON:
       case PRIM_BLOCK_BEGIN_ON:
       case PRIM_BLOCK_COBEGIN_ON:
       case PRIM_BLOCK_COFORALL_ON:
        if (call->parentSymbol)
          INT_FATAL("Primitive should no longer be in AST");
        break;
       default:
        break;
      }
}

static void
checkLowerIteratorsRemovedPrims()
{
  for_alive_in_Vec(CallExpr, call, gCallExprs)
    if (call->primitive)
      switch(call->primitive->tag)
      {
       case PRIM_YIELD:
        if (call->parentSymbol)
          INT_FATAL("Primitive should no longer be in AST");
        break;
       default:
        break;
      }
}

// Some flags imply other flags.
// Make sure that these relations always hold.
static void
checkFlagRelationships()
{
  for_alive_in_Vec(DefExpr, def, gDefExprs)
  {
    // These tests apply to function symbols.
    if (FnSymbol* fn = toFnSymbol(def->sym))
    {
      // FLAG_EXTERN => FLAG_LOCAL_ARGS
      INT_ASSERT(!fn->hasFlag(FLAG_EXTERN) || fn->hasFlag(FLAG_LOCAL_ARGS));

      // FLAG_EXPORT => FLAG_LOCAL_ARGS
      INT_ASSERT(!fn->hasFlag(FLAG_EXPORT) || fn->hasFlag(FLAG_LOCAL_ARGS));
    }
  }
}

static void
checkAutoCopyMap()
{
  Vec<Type*> keys;
  getAutoCopyTypeKeys(keys);
  forv_Vec(Type, key, keys)
  {
    if (hasAutoCopyForType(key)) {
      FnSymbol* fn = getAutoCopyForType(key);
      if (fn->numFormals() > 1) {
        Type* baseType = fn->getFormal(1)->getValType();
        INT_ASSERT(baseType == key);

        sanityCheckDefinedConstArg(fn->getFormal(2));
      }
    }
  }
}

static FnSymbol* findUserInitEq(AggregateType* at) {
  for_alive_in_Vec(FnSymbol, fn, gFnSymbols) {
    if (fn->name == astrInitEquals &&
        !fn->hasFlag(FLAG_COMPILER_GENERATED) &&
        fn->numFormals() >= 1) {
      ArgSymbol* lhs = fn->getFormal(1);
      Type* t = lhs->getValType();
      if (t == at)
        return fn;
    }
  }
  return NULL;
}

static FnSymbol* findUserAssign(AggregateType* at) {

  for_alive_in_Vec(FnSymbol, fn, gFnSymbols) {
    if (fn->name == astrSassign &&
        !fn->hasFlag(FLAG_COMPILER_GENERATED) &&
        fn->numFormals() >= 1) {
      ArgSymbol* lhs = fn->getFormal(1);
      Type* t = lhs->getValType();
      if (t == at)
        return fn;
    }
  }
  return NULL;
}


static void checkInitEqAssignCast()
{
  for_alive_in_Vec(TypeSymbol, ts, gTypeSymbols) {
    if (ts->hasFlag(FLAG_EXTERN))
      continue; // can't make init= for extern types today anyway
    if (!isRecord(ts->type) && !isUnion(ts->type))
      continue; // can't make init= for non-records non-unions today anyway

    AggregateType* at = toAggregateType(ts->type);
    bool defaultInitEq = ts->hasFlag(FLAG_TYPE_DEFAULT_INIT_EQUAL);
    bool customInitEq = ts->hasFlag(FLAG_TYPE_CUSTOM_INIT_EQUAL);
    bool defaultAssign = ts->hasFlag(FLAG_TYPE_DEFAULT_ASSIGN);
    bool customAssign = ts->hasFlag(FLAG_TYPE_CUSTOM_ASSIGN);
    if (defaultInitEq && customAssign) {
      USR_FATAL_CONT(ts, "Type '%s' uses compiler-generated default 'init=' "
                         "but has a custom '=' function. "
                         "Please add an 'init=' method",
                         toString(ts->type));
      if (FnSymbol* userAssign = findUserAssign(at))
        USR_PRINT(userAssign, "'=' for '%s' defined here", toString(ts->type));
    }
    if (defaultInitEq && customInitEq) {
      USR_FATAL_CONT(ts, "Type '%s' uses compiler-generated default 'init=' "
                         "but also has a custom 'init=' method. "
                         "Please add an 'init=' method with the same RHS type",
                         toString(ts->type));
      if (FnSymbol* userInitEq = findUserInitEq(at))
        USR_PRINT(userInitEq, "'init=' for '%s' defined here", toString(ts->type));
    }

    if (customInitEq && defaultAssign) {
      USR_FATAL_CONT(ts, "Type '%s' uses compiler-generated default '=' "
                         "but has a custom 'init=' method. "
                         "Please add a '=' function.",
                         toString(ts->type));
      if (FnSymbol* userInitEq = findUserInitEq(at))
        USR_PRINT(userInitEq, "'init=' for '%s' defined here", toString(ts->type));
    }
    if (defaultAssign && customAssign) {
      USR_FATAL_CONT(ts, "Type '%s' uses compiler-generated default '=' "
                         "but also has a custom '=' function. "
                         "Please add a '=' function with the same RHS type.",
                         toString(ts->type));
      if (FnSymbol* userAssign = findUserAssign(at))
        USR_PRINT(userAssign, "'=' for '%s' defined here", toString(ts->type));
    }
  }
}

// There are some compiler-generated chpl__deserialize calls that we pretend to
// be resolved, however, they have formal-actual type mismatches. We want to
// ignore them. They will be removed later in compilation.
//
// They must be inside a CondStmt's else block, where the condExpr has
// FLAG_DESERIALIZATION_BLOCK_MARKER.
static bool
isTemporaryDeserializeCall(CallExpr* call) {
  if (! call->isNamed("chpl__deserialize")) {
    return false;
  }

  Expr* parent = call->parentExpr;
  BlockStmt* parentBlock = NULL;
  do {
    if (parentBlock == NULL) {  // first note the parent block
      if (BlockStmt* block = toBlockStmt(parent)) {
        parentBlock = block;
      }
    }
    else {  // we have it, now find the conditional
      if (CondStmt* cond = toCondStmt(parent)) {
        if (cond->elseStmt == parentBlock) {
          if (SymExpr* condSE = toSymExpr(cond->condExpr)) {
            if (condSE->symbol()->hasFlag(FLAG_DESERIALIZATION_BLOCK_MARKER)) {
              return true;
            }
          }
        }
      }
    }
    parent = parent->parentExpr;
  } while (parent != NULL);

  return false;
}


// TODO: Can this be merged with checkFormalActualTypesMatch()?
static void
checkFormalActualBaseTypesMatch()
{
  for_alive_in_Vec(CallExpr, call, gCallExprs)
  {
    if (! call->parentSymbol)
      // Call is not in tree
      continue;

    // Only look at calls in functions that have been resolved.
    if (! call->parentSymbol->hasFlag(FLAG_RESOLVED))
      continue;

    // Skip verifying some degenerate chpl__deserialize calls
    if (isTemporaryDeserializeCall(call))
      continue;

    if (FnSymbol* fn = call->resolvedFunction())
    {
      if (fn->hasFlag(FLAG_EXTERN))
        continue;

      if (! fn->hasFlag(FLAG_RESOLVED))
        continue;

      for_formals_actuals(formal, actual, call)
      {
        if (actual->typeInfo() == dtNil) {
          if (formal->type == dtNil)
            // Exact match, so OK.
            continue;

          if (isClassLikeOrPtr(formal->type))
            // dtNil can be converted to any class type, so OK.
            continue;

          // All other cases == error.
          INT_FATAL(call, "nil is passed to the formal %s of a non-class type",
                    formal->name);
        }

        if (formal->type->getValType() != actual->typeInfo()->getValType())
          INT_FATAL(call,
                    "actual formal type mismatch for %s: %s != %s",
                    fn->name,
                    actual->typeInfo()->symbol->name,
                    formal->type->symbol->name);
      }
    }
  }
}

// After resolution the retType field is just a cached version of the type of
// the return value variable.
static void
checkRetTypeMatchesRetVarType() {
  for_alive_in_Vec(FnSymbol, fn, gFnSymbols) {

    // Iterators break this rule.
    // retType is the type of the iterator record
    // The return value type is the type of the index the iterator returns.
    if (fn->isIterator()) continue;

    // auto ii and thunk invoke functions break this rule, but only during the time that
    // they are prototypes.  After the body is filled in, they should obey it.
    // But, for some of them, the body is never filled in.
    if (fn->hasFlag(FLAG_AUTO_II) || fn->hasFlag(FLAG_THUNK_INVOKE)) continue;

    // No body, so no return symbol.
    if (fn->hasFlag(FLAG_NO_FN_BODY)) continue;

    INT_ASSERT(fn->retType == fn->getReturnSymbol()->type);
  }
}

static void
checkFormalActualTypesMatch()
{
  for_alive_in_Vec(CallExpr, call, gCallExprs)
  {
    // Skip verifying some degenerate chpl__deserialize calls
    if (isTemporaryDeserializeCall(call))
      continue;

    if (FnSymbol* fn = call->resolvedFunction())
    {
      if (fn->hasFlag(FLAG_EXTERN))
        continue;

      for_formals_actuals(formal, actual, call)
      {
        if (actual->typeInfo() == dtNil) {
          if (formal->type == dtNil)
            // Exact match, so OK.
            continue;

          if (isClassLikeOrPtr(formal->type))
            // dtNil can be converted to any class type, so OK.
            continue;

          // All other cases == error.
          INT_FATAL(call, "nil is passed to the formal %s of a non-class type",
                    formal->name);
        }

        if (SymExpr* se = toSymExpr(actual)) {
          if (se->symbol() == gDummyRef && formal->hasFlag(FLAG_RETARG))
            // The compiler generates this combination.
            continue;
        }

        // Allow raw_c_void_ptr/c_ptr(void) mismatch. Although implicit
        // conversion between the two is allowed, the compiler currently inserts
        // function such as chpl_here_free using raw_c_void_ptr after
        // resolution.
        // TODO: Remove this once we are using c_ptr(void) everywhere in the
        // compiler and no longer have raw_c_void_ptr (dtCVoidPtr) sticking
        // around.
        if (isCVoidPtr(actual->typeInfo()) && isCVoidPtr(formal->type)) {
          continue;
        }

        if ((isCPtrConstChar(formal->getValType()) ||
             isCPtrConstChar(actual->getValType())) &&
            (formal->getValType()==dtStringC ||
             actual->getValType()==dtStringC)) {
            // we allow conversion between these types in function resolution
            // TODO: remove this once we get rid of c_string remnants
            continue;
        }

        if (formal->getValType() != actual->getValType()) {
          INT_FATAL(call,
                    "actual formal type mismatch for %s: %s != %s",
                    fn->name,
                    actual->typeInfo()->symbol->name,
                    formal->type->symbol->name);
        }
      }
    }
  }
}
