/*
 * Copyright 2024-2025 Hewlett Packard Enterprise Development LP
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

#include "chpl/types/InterfaceType.h"
#include "chpl/framework/query-impl.h"
#include "chpl/parsing/parsing-queries.h"
#include "chpl/resolution/can-pass.h"
#include "chpl/uast/Interface.h"

namespace chpl {
namespace types {

bool InterfaceType::validateSubstitutions(Context* context,
                                          const ID& id,
                                          SubstitutionsMap& subs) {
  // Just a generic instance of the interface
  if (subs.empty()) return true;

  auto ast = parsing::idToAst(context, id);
  if (!ast) return false;
  auto ifc = ast->toInterface();
  if (!ifc) return false;

  CHPL_ASSERT(ifc->numFormals() >= 0);
  if (subs.size() != (size_t) ifc->numFormals()) return false;
  for (auto fml : ifc->formals()) {
    if (subs.count(fml->id()) == 0) return false;
  }

  return true;
}

owned<InterfaceType> const&
InterfaceType::getInterfaceType(Context* context, ID id, UniqueString name, SubstitutionsMap subs) {
  QUERY_BEGIN(getInterfaceType, context, id, name, subs);
  CHPL_ASSERT(validateSubstitutions(context, id, subs));
  auto result = toOwned(new InterfaceType(id, name, subs));
  return QUERY_END(result);
}

const InterfaceType* InterfaceType::get(Context* context, ID id, UniqueString name, SubstitutionsMap subs) {
  return getInterfaceType(context, id, name, subs).get();
}

const InterfaceType* InterfaceType::getContextManagerType(Context* context) {
  auto [id, name] =
      parsing::getSymbolFromTopLevelModule(context, "ChapelContext", "contextManager");
  return InterfaceType::get(context, id, name, SubstitutionsMap());
}

static const InterfaceType* const&
interfaceTypeWithTypesQuery(Context* context,
                            const InterfaceType* ift,
                            std::vector<types::QualifiedType> types) {
  QUERY_BEGIN(interfaceTypeWithTypesQuery, context, ift, types);
  const InterfaceType* res = nullptr;

  if (ift->substitutions().size() > 0) {
    // don't allow instantiating already-instantiated interfaces
  } else {
    auto ast = parsing::idToAst(context, ift->id());
    CHPL_ASSERT(ast);
    auto itf = ast->toInterface();
    CHPL_ASSERT(itf);

    CHPL_ASSERT(itf->numFormals() >= 0);
    if (types.size() != (size_t) itf->numFormals()) {
      // not good, wrong instantiation
    } else {
      InterfaceType::SubstitutionsMap subs;
      auto typesIt = types.begin();
      for (auto formal : itf->formals()) {
        // Force the intent to TYPE
        auto newType = QualifiedType(QualifiedType::TYPE, (typesIt++)->type());
        subs.emplace(formal->id(), std::move(newType));
      }

      res = InterfaceType::get(context, itf->id(), itf->name(), std::move(subs));
    }
  }

  return QUERY_END(res);
}

const InterfaceType* InterfaceType::withTypes(Context* context,
                                              const InterfaceType* ift,
                                              std::vector<types::QualifiedType> types) {
  return interfaceTypeWithTypesQuery(context, ift, std::move(types));
}

static const ID& idForInterfaceAssociatedTypeQuery(Context* context, ID itfId, UniqueString name) {
  QUERY_BEGIN(idForInterfaceAssociatedTypeQuery, context, itfId, name);
  auto ast = parsing::idToAst(context, itfId);
  CHPL_ASSERT(ast);
  auto itf = ast->toInterface();
  CHPL_ASSERT(itf);

  ID res;
  for (auto child : itf->children()) {
    auto td = child->toVariable();
    if (!td) continue;
    CHPL_ASSERT(td->storageKind() == QualifiedType::TYPE);
    if (td->name() != name) continue;

    res = td->id();
    break;
  }

  return QUERY_END(res);
}

const ID& InterfaceType::idForAssociatedType(Context* context, UniqueString name) const {
  return idForInterfaceAssociatedTypeQuery(context, id_, name);
}

bool InterfaceType::isInstantiationOf(Context* context, const InterfaceType* other) const {
  if (id_ != other->id()) return false;
  CHPL_ASSERT(name_ == other->name());
  return resolution::canInstantiateSubstitutions(context, subs_, other->substitutions(), /* allowMissing */ false);
}

void InterfaceType::stringify(std::ostream& ss, StringifyKind stringKind) const {
  ss << name_;
  if (!subs_.empty()) {
    ss << "(";
    CompositeType::stringifySubstitutions(ss, stringKind, subs_);
    ss << ")";
  }
}

} // end namespace types
} // end namespace chpl
