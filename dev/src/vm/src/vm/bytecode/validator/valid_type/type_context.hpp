// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <base/except/exceptions.hpp>

#include <vm/bytecode/bytecode.hpp>
#include <vm/bytecode/type_of_data.hpp>
#include <vm/bytecode/validator/valid_type/valid_type.hpp>
#include <vm/utils/stable_obj_id_name_map.hpp>

#include <vector>

namespace vm::code {
	/**
	 * @brief Stores the declarations of a program and the types built from them.
	 *
	 * @note A container only. `TypeResolver` decides what to build and builds it, so that this
	 * knows neither how a type is declared nor how one is referred to.
	 */
	class TypeContext final {
	public:
		/**
		 * @brief Stores a declaration, so that the type it names can be built.
		 * @return Whether the name is new. A declaration already stored verbatim is accepted and
		 * reported as not new.
		 * @throws DuplicatedTypeError if the name is already declared with different contents.
		 */
		bool addDeclaration(const TypeOfData& type);

		/// @brief The declaration stored under `name`, if any.
		[[nodiscard]] base::Optional<CRef<TypeOfData>> findDeclaration(base::StrID name) const;

		/**
		 * @brief Stores a new, not yet defined type for `expr`.
		 * @pre No type is stored for `expr` yet.
		 */
		valid_type::ValidTypeID declare(const TypeExpr& expr);

		/// @brief The type stored under `id`, to define.
		[[nodiscard]] valid_type::ValidType& forDefinition(valid_type::ValidTypeID id);

		/**
		 * @brief Finalizes the type stored under `id`.
		 * @pre Every type it refers to is defined.
		 */
		void finalize(valid_type::ValidTypeID id);

		[[nodiscard]] const valid_type::ValidTypeMap& getCurrentTypes() const;

		[[nodiscard]] const ObjIdNameMap<TypeOfData>& getTodTypes() const;

	private:
		ObjIdNameMap<TypeOfData> tod_types;

		valid_type::ValidTypeMap types;
	};
}
