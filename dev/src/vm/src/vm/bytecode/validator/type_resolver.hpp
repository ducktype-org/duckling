// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

/**
 * @file type_resolver.hpp
 * @brief Turning a type expression into the type it denotes.
 *
 * Mapping an expression onto a stored type belongs here rather than in its callers, so that the
 * `TypeContext` can stay a plain container and every consumer agrees on the mapping.
 */

#pragma once

#include <base/collections/optional.hpp>

#include <vm/bytecode/type_expr.hpp>
#include <vm/bytecode/type_of_data.hpp>
#include <vm/bytecode/validator/valid_type/type_context.hpp>
#include <vm/bytecode/validator/valid_type/valid_type.hpp>

#include <vector>

namespace vm::code {
	/**
	 * @brief The type an expression denotes, or an empty optional if no such type is stored.
	 * @note Never stores anything, so it is the form to use once validation has run.
	 */
	[[nodiscard]] base::Optional<CRef<valid_type::ValidType>> findResolved(
		const valid_type::ValidTypeMap& types, const TypeExpr& expr
	);

	/**
	 * @brief Resolves type expressions against a `TypeContext`, building each type the first
	 * time an expression denotes it.
	 *
	 * A type is declared before what it refers to is resolved, so that types can refer to
	 * themselves, and defined afterwards. The types built are finalized together by
	 * `finalizeBuilt`, once every one of them is defined.
	 */
	class TypeResolver final {
	public:
		explicit TypeResolver(TypeContext& context): context(&context) {}

		/**
		 * @brief The id of the type `expr` denotes, building it if this is its first use.
		 * @throws UnknownTypeError if the expression names no declared type.
		 */
		[[nodiscard]] valid_type::ValidTypeID resolve(const TypeExpr& expr);

		/// @brief Finalizes every type built so far. Call it once nothing more is resolved.
		void finalizeBuilt();

	private:
		/// @brief Defines the type declared under `id` from its declaration.
		void define(valid_type::ValidTypeID id, const TypeOfData& declaration);

		Ref<TypeContext>                     context;
		std::vector<valid_type::ValidTypeID> built;
	};
}
