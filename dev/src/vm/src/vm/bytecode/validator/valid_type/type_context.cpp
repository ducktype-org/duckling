// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "type_context.hpp"

#include <string_id/string_id.hpp>

#include <vm/bytecode/type_of_data.hpp>
#include <vm/bytecode/validator/errors.hpp>
#include <vm/bytecode/validator/valid_type/valid_type.hpp>
#include <vm/bytecode/validator/valid_type/valid_type_id.hpp>

using namespace vm::code;

bool TypeContext::addDeclaration(const TypeOfData& type) {
	const auto name = typeName(type);
	if (tod_types.contains(name)) {
		// The same declaration twice is accepted; two different ones under one name are not.
		if (type != *tod_types.at(name)) throw DuplicatedTypeError(type, *tod_types.at(name));
		return false;
	}

	tod_types.insert(type, name);
	return true;
}

base::Optional<CRef<TypeOfData>> TypeContext::findDeclaration(base::StrID name) const {
	return tod_types.atMaybe(name);
}

valid_type::ValidTypeID TypeContext::declare(const TypeExpr& expr) {
	const base::StrID name = expr.canonicalName();
	CORE_ASSERT(!types.contains(name), "A type is already stored for `", name, "`");

	const auto id = valid_type::ValidTypeID(types.size());
	types.insert(valid_type::ValidType::declareType(expr, id), name);
	return id;
}

valid_type::ValidType& TypeContext::forDefinition(valid_type::ValidTypeID id) {
	return *types.at(id);
}

void TypeContext::finalize(valid_type::ValidTypeID id) { types.at(id)->finalize(types); }

const valid_type::ValidTypeMap& TypeContext::getCurrentTypes() const { return types; }

const vm::ObjIdNameMap<TypeOfData>& vm::code::TypeContext::getTodTypes() const { return tod_types; }
