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

#include "test-parsing.h"

#include "chpl/parsing/Parser.h"
#include "chpl/framework/Context.h"
#include "chpl/uast/AstNode.h"
#include "chpl/uast/Begin.h"
#include "chpl/uast/Block.h"
#include "chpl/uast/Comment.h"
#include "chpl/uast/Identifier.h"
#include "chpl/uast/Module.h"
#include "chpl/uast/TaskVar.h"

static void test0(Parser* parser) {
  ErrorGuard guard(parser->context());
  auto parseResult = parseStringAndReportErrors(parser, "test0.chpl",
      "/* comment 1 */\n"
      "begin /* comment 2 */\n"
      "with /* comment 3 */ (ref a, var b = foo()) /* comment 4 */ {\n"
      "  /* comment 5 */\n"
      "  writeln(a);\n"
      "  /* comment 6 */\n"
      "}\n"
      "/* comment 7 */\n");
  assert(!guard.realizeErrors());
  auto mod = parseResult.singleModule();
  assert(mod);
  assert(mod->numStmts() == 3);
  assert(mod->stmt(0)->isComment());
  assert(mod->stmt(1)->isBegin());
  assert(mod->stmt(2)->isComment());
  const Begin* begin = mod->stmt(1)->toBegin();
  assert(begin != nullptr);
  const WithClause* withClause = begin->withClause();
  assert(withClause);
  assert(withClause->numExprs() == 2);
  auto tv1 = withClause->expr(0)->toTaskVar();
  assert(tv1 && tv1->intent() == TaskVar::REF);
  assert(!tv1->typeExpression() && !tv1->initExpression());
  auto tv2 = withClause->expr(1)->toTaskVar();
  assert(tv2 && tv2->intent() == TaskVar::VAR);
  assert(!tv2->typeExpression() && tv2->initExpression());
  assert(begin->blockStyle() == BlockStyle::EXPLICIT);
  assert(begin->numStmts() == 3);
  assert(begin->stmt(0)->isComment());
  assert(begin->stmt(0)->toComment()->str() == "/* comment 5 */");
  assert(begin->stmt(1)->isFnCall());
  assert(begin->stmt(2)->isComment());
}

static void test1(Parser* parser) {
  ErrorGuard guard(parser->context());
  auto parseResult = parseStringAndReportErrors(parser, "test1.chpl",
      "/* comment 1 */\n"
      "begin /* comment 2 */ {\n"
      "  /* comment 5 */\n"
      "  writeln(a);\n"
      "  /* comment 6 */\n"
      "}\n"
      "/* comment 7 */\n");
  assert(!guard.realizeErrors());
  auto mod = parseResult.singleModule();
  assert(mod);
  assert(mod->numStmts() == 3);
  assert(mod->stmt(0)->isComment());
  assert(mod->stmt(1)->isBegin());
  assert(mod->stmt(2)->isComment());
  const Begin* begin = mod->stmt(1)->toBegin();
  assert(begin != nullptr);
  assert(!begin->withClause());
  assert(begin->blockStyle() == BlockStyle::EXPLICIT);
  assert(begin->numStmts() == 3);
  assert(begin->stmt(0)->isComment());
  assert(begin->stmt(0)->toComment()->str() == "/* comment 5 */");
  assert(begin->stmt(1)->isFnCall());
  assert(begin->stmt(2)->isComment());
}

static void test2(Parser* parser) {
  ErrorGuard guard(parser->context());
  auto parseResult = parseStringAndReportErrors(parser, "test2.chpl",
      "/* comment 1 */\n"
      "begin /* comment 2 */\n"
      "with /* comment 3 */ (ref a, var b = foo()) /* comment 4 */\n"
      "  /* comment 5 */\n"
      "  writeln(a);\n"
      "/* comment 7 */\n");
  assert(!guard.realizeErrors());
  auto mod = parseResult.singleModule();
  assert(mod);
  assert(mod->numStmts() == 3);
  assert(mod->stmt(0)->isComment());
  assert(mod->stmt(1)->isBegin());
  assert(mod->stmt(2)->isComment());
  const Begin* begin = mod->stmt(1)->toBegin();
  assert(begin != nullptr);
  const WithClause* withClause = begin->withClause();
  assert(withClause);
  assert(withClause->numExprs() == 2);
  auto tv1 = withClause->expr(0)->toTaskVar();
  assert(tv1 && tv1->intent() == TaskVar::REF);
  assert(!tv1->typeExpression() && !tv1->initExpression());
  auto tv2 = withClause->expr(1)->toTaskVar();
  assert(tv2 && tv2->intent() == TaskVar::VAR);
  assert(!tv2->typeExpression() && tv2->initExpression());
  assert(begin->blockStyle() == BlockStyle::IMPLICIT);
  assert(begin->numStmts() == 3);
  assert(begin->stmt(0)->isComment());
  assert(begin->stmt(0)->toComment()->str() == "/* comment 4 */");
  assert(begin->stmt(1)->isComment());
  assert(begin->stmt(2)->isFnCall());
}

static void test3(Parser* parser) {
  ErrorGuard guard(parser->context());
  auto parseResult = parseStringAndReportErrors(parser, "test3.chpl",
      "begin with (re A) { }\n"
      "begin with () { }\n"
      "begin with ref A { }\n");
  auto numErrors = 5;
  assert(guard.errors().size() == (size_t) numErrors);
  assert("invalid intent expression in 'with' clause" == guard.error(1)->message());
  assert("'with' clause cannot be empty" == guard.error(2)->message());
  assert("missing parentheses around 'with' clause intents" == guard.error(4)->message());
  // The other errors are from the parser as "near ...".
  // It would be really nice to not have those be emitted at all.
  assert(guard.realizeErrors() == numErrors);
}

int main() {
  Context context;
  Context* ctx = &context;

  auto parser = Parser::createForTopLevelModule(ctx);
  Parser* p = &parser;

  test0(p);
  test1(p);
  test2(p);
  test3(p);

  return 0;
}
