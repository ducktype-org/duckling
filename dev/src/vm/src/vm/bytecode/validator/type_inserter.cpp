// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

/**
 * @file type_inserter.cpp
 * @brief Adding declared types to a program.
 */

#include <string_id/string_id.hpp>

#include <vm/bytecode/type_of_data.hpp>
#include <vm/bytecode/validator/errors.hpp>
#include <vm/bytecode/validator/type_inserter.hpp>
#include <vm/bytecode/validator/type_resolver.hpp>
#include <vm/bytecode/validator/type_validator.hpp>
#include <vm/bytecode/validator/valid_type/valid_type.hpp>
#include <vm/bytecode/validator/valid_type/valid_type_id.hpp>

#include <ranges>
#include <tuple>
#include <vector>

using namespace vm::code;

void vm::code::detail::insertTypes(
	TypeContext&                                     context,
	const std::vector<TypeOfData>&                   new_types,
	const base::HashMap<base::StrID, FuncSignature>& function_signatures
) {
	std::vector<base::StrID> new_names;
	for (const auto& type: new_types)
		if (context.addDeclaration(type)) new_names.push_back(typeName(type));

	const auto&              tod_types = context.getTodTypes();
	const std::vector<usize> new_ids
		= new_names
	    | std::views::transform([&](base::StrID name) { return tod_types.idOf(name).value(); })
	    | std::ranges::to<std::vector>();
	detail::validateTypes(tod_types, new_ids, function_signatures);

	// Every declaration is built, including one that nothing refers to, so that each is checked.
	TypeResolver resolver(context);
	for (const auto name: new_names) std::ignore = resolver.resolve(TypeExpr::type(name));
	resolver.finalizeBuilt();
}
