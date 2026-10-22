// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <vm/bytecode/type_of_data.hpp>
#include <vm/bytecode/validator/errors.hpp>

namespace vm::code::detail {
	using namespace vm::code;
	using FieldVector = std::vector<std::pair<base::StrID, vm::TypeRef>>;

	template<typename T>
	concept InheritableTypeConcept
		= std::is_same_v<T, ClassType> || std::is_same_v<T, InterfaceType>;
	template<typename T>
	concept FieldableTypeConcept = std::is_same_v<T, ClassType> || std::is_same_v<T, DataType>;
	template<typename F>
	concept ErrorFactoryConcept
		= std::invocable<F> && std::is_base_of_v<ValidationError, std::invoke_result_t<F>>;
	template<typename T>
	concept TypeOfDataConcept = std::is_constructible_v<TypeOfData, T>;
}  // namespace vm::code::detail
