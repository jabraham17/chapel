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

#include "chpl/types/CompositeType.h"

#include "chpl/parsing/parsing-queries.h"
#include "chpl/resolution/can-pass.h"
#include "chpl/resolution/resolution-queries.h"
#include "chpl/resolution/resolution-types.h"
#include "chpl/types/BasicClassType.h"
#include "chpl/types/ClassType.h"
#include "chpl/types/ClassTypeDecorator.h"
#include "chpl/types/CPtrType.h"
#include "chpl/types/DomainType.h"
#include "chpl/types/RecordType.h"
#include "chpl/types/TupleType.h"
#include "chpl/uast/Decl.h"
#include "chpl/uast/NamedDecl.h"

namespace chpl {
namespace types {


using namespace resolution;

bool
CompositeType::areSubsInstantiationOf(Context* context,
                                      const CompositeType* partial) const {
  // Note: Assumes 'this' and 'partial' share a root instantiation.
  return canInstantiateSubstitutions(context,
                                     substitutions(),
                                     partial->substitutions(),
                                     /* allowMissing */ !partial->isTupleType());
}

CompositeType::~CompositeType() {
}

using SubstitutionPair = CompositeType::SubstitutionPair;

static void stringifySortedSubstitutions(std::ostream& ss,
                                         chpl::StringifyKind stringKind,
                                         const std::vector<SubstitutionPair>& sorted,
                                         bool& emittedField) {
  for (const auto& sub : sorted) {
    if (emittedField) ss << ", ";

    if (stringKind != StringifyKind::CHPL_SYNTAX) {
      sub.first.stringify(ss, stringKind);
      ss << ":";
      sub.second.stringify(ss, stringKind);
    } else {
      if (sub.second.isType() || (sub.second.isParam() && sub.second.param() == nullptr)) {
        sub.second.type()->stringify(ss, stringKind);
      } else if (sub.second.isParam()) {
        sub.second.param()->stringify(ss, stringKind);
      } else {
        // Some odd configuration; fall back to printing the qualified type.
        CHPL_UNIMPL("attempting to stringify odd type representation as Chapel syntax");
        sub.second.stringify(ss, stringKind);
      }
    }

    emittedField = true;
  }
}

static
std::vector<SubstitutionPair>
sortedSubstitutionsMap(const CompositeType::SubstitutionsMap& subs) {
  // since it's an unordered map, iteration will occur in a
  // nondeterministic order.
  // it's important to sort the keys / iterate in a deterministic order here,
  // so we create a vector of pair<K,V> and sort that instead
  std::vector<SubstitutionPair> v(subs.begin(), subs.end());
  std::sort(v.begin(), v.end(), FirstElementComparator<ID, QualifiedType>());
  return v;
}

void CompositeType::stringifySubstitutions(std::ostream& ss,
                                           chpl::StringifyKind stringKind,
                                           const SubstitutionsMap& subs) {
  bool emittedField = false;
  auto sorted = sortedSubstitutionsMap(subs);
  stringifySortedSubstitutions(ss, stringKind, sorted, emittedField);
}
void CompositeType::stringify(std::ostream& ss,
                              chpl::StringifyKind stringKind) const {
  // compute the parent class type for BasicClassType
  const Type* superType = nullptr;
  if (auto bct = this->toBasicClassType()) {
    superType = bct->parentClassType();
  }

  if (isStringType()) {
    ss << "string";
  } else if (isBytesType()) {
    ss << "bytes";
  } else if (isLocaleType()) {
    ss << "locale";
  } else if (id().symbolPath() == USTR("ChapelRange._range")) {
    ss << "range";
  } else {
    name().stringify(ss, stringKind);
  }

  auto sorted = sortedSubstitutions();

  bool printSupertype =
    superType != nullptr && stringKind != StringifyKind::CHPL_SYNTAX;

  // Prepend parent substitutions to 'sorted' list
  if (!printSupertype && superType != nullptr) {
    auto cur = superType->toBasicClassType();
    while (cur != nullptr) {
      auto parentSubs = cur->getCompositeType()->sortedSubstitutions();
      sorted.insert(sorted.begin(), parentSubs.begin(), parentSubs.end());
      cur = cur->parentClassType();
    }
  }

  if (printSupertype || !sorted.empty()) {
    bool emittedField = false;
    ss << "(";

    if (printSupertype) {
      ss << "super:";
      superType->stringify(ss, stringKind);
      emittedField = true;
    }

    stringifySortedSubstitutions(ss, stringKind, sorted, emittedField);
    ss << ")";
  }
}

const RecordType* CompositeType::getStringType(Context* context) {
  auto [id, name] =
      parsing::getSymbolFromTopLevelModule(context, "String", "_string");
  return RecordType::get(context, id, name,
                         /* instantiatedFrom */ nullptr, SubstitutionsMap());
}

const RecordType* CompositeType::getRangeType(Context* context) {
  auto [id, name] =
      parsing::getSymbolFromTopLevelModule(context, "ChapelRange", "_range");
  return RecordType::get(context, id, name,
                         /* instantiatedFrom */ nullptr, SubstitutionsMap());
}

const RecordType* CompositeType::getBytesType(Context* context) {
  auto [id, name] =
      parsing::getSymbolFromTopLevelModule(context, "Bytes", "_bytes");
  return RecordType::get(context, id, name,
                         /* instantiatedFrom */ nullptr, SubstitutionsMap());
}

const RecordType* CompositeType::getLocaleType(Context* context) {
  auto [id, name] =
      parsing::getSymbolFromTopLevelModule(context, "ChapelLocale", "_locale");
  return RecordType::get(context, id, name,
                         /* instantiatedFrom */ nullptr,
                         SubstitutionsMap());
}

const RecordType* CompositeType::getLocaleIDType(Context* context) {
  auto [id, name] = parsing::getSymbolFromTopLevelModule(
      context, "LocaleModelHelpRuntime", "chpl_localeID_t");
  return RecordType::get(context, id, name,
                         /* instantiatedFrom */ nullptr,
                         SubstitutionsMap());
}

const RecordType* CompositeType::getDistributionType(Context* context) {
  auto [id, name] = parsing::getSymbolFromTopLevelModule(
      context, "ChapelDistribution", "_distribution");
  return RecordType::get(context, id, name,
                         /* instantiatedFrom */ nullptr, SubstitutionsMap());
}

static const ID getOwnedRecordId(Context* context) {
  return parsing::getSymbolIdFromTopLevelModule(context, "OwnedObject",
                                                "_owned");
}

static const ID getSharedRecordId(Context* context) {
  return parsing::getSymbolIdFromTopLevelModule(context, "SharedObject",
                                                "_shared");
}

static const RecordType* tryCreateManagerRecord(Context* context,
                                                const ID& recordId,
                                                const BasicClassType* bct) {
  const RecordType* instantiatedFrom = nullptr;
  SubstitutionsMap subs;
  if (bct != nullptr) {
    instantiatedFrom = tryCreateManagerRecord(context,
                                              recordId,
                                              /*bct*/ nullptr);

    // Note: We know these types aren't nested, and so don't require a proper
    // ResolutionContext at this time.
    ResolutionContext rc(context);
    auto fields = fieldsForTypeDecl(&rc,
                                    instantiatedFrom,
                                    DefaultsPolicy::IGNORE_DEFAULTS);
    for (int i = 0; i < fields.numFields(); i++) {
      if (fields.fieldName(i) != "chpl_t") continue;
      auto ctd = ClassTypeDecorator(ClassTypeDecorator::BORROWED_NONNIL);
      auto ct = ClassType::get(context, bct, /* manager */ nullptr, ctd);

      subs[fields.fieldDeclId(i)] = QualifiedType(QualifiedType::TYPE, ct);
      break;
    }
  }

  auto name = recordId.symbolName(context);
  return RecordType::get(context, recordId, name,
                         instantiatedFrom,
                         std::move(subs));
}

const RecordType*
CompositeType::getOwnedRecordType(Context* context, const BasicClassType* bct) {
  return tryCreateManagerRecord(context, getOwnedRecordId(context), bct);
}

const RecordType*
CompositeType::getSharedRecordType(Context* context, const BasicClassType* bct) {
  return tryCreateManagerRecord(context, getSharedRecordId(context), bct);
}

const ClassType* CompositeType::getErrorType(Context* context) {
  auto [id, name] =
      parsing::getSymbolFromTopLevelModule(context, "Errors", "Error");
  auto dec = ClassTypeDecorator(ClassTypeDecorator::GENERIC_NONNIL);
  auto bct = BasicClassType::get(context, id,
                                name,
                                BasicClassType::getRootClassType(context),
                                /* instantiatedFrom */ nullptr,
                                SubstitutionsMap());
  return ClassType::get(context, bct, /* manager */ nullptr, dec);
}

std::vector<SubstitutionPair> CompositeType::sortedSubstitutions(void) const {
  return sortedSubstitutionsMap(subs_);
}

size_t hashSubstitutionsMap(const CompositeType::SubstitutionsMap& subs) {
  return hashUnorderedMap(subs);
}

void stringifySubstitutionsMap(std::ostream& streamOut,
                               StringifyKind stringKind,
                               const CompositeType::SubstitutionsMap& subs) {
  auto sorted = sortedSubstitutionsMap(subs);
  bool first = true;
  for (auto const& x : sorted)
  {
    ID id = x.first;
    QualifiedType qt = x.second;

    if (first) {
      first = false;
    } else {
      streamOut << ", ";
    }

    id.stringify(streamOut, stringKind);

    streamOut << "= ";

    qt.stringify(streamOut, stringKind);
  }
}


} // end namespace types
} // end namespace chpl
