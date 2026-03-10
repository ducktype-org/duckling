#pragma once

#include <base/except/exceptions.hpp>

#include <vm/bytecode/bytecode.hpp>
#include <vm/bytecode/type_of_data.hpp>
#include <vm/bytecode/validator/valid_type/valid_type.hpp>
#include <vm/utils/stable_obj_id_name_map.hpp>

namespace vm::code {
	/**
	 * @brief A simple container for types which doesn't allow duplicates. This structure is the
	 * main entry point for type verification and building.
	 * @note `TypeContext` always contains a valid set of types if `insertAndValidate` didn't throw
	 * any errors.
	 */
	class TypeContext final {
	public:
		/**
		 * @brief Inserts new types.
		 * If a type is duplicated throws DuplicatedTypeError.
		 * Throws an exception if inserting new types would invalidate the TypeContext in the
		 * context of function_signatures.
		 * @note After an exception is thrown, the TypeContext's state is undefined.
		 */
		void insertAndValidate(
			const std::vector<TypeOfData>&                   new_types,
			const base::HashMap<base::StrID, FuncSignature>& function_signatures
		);

		[[nodiscard]] const valid_type::ValidTypeMap& getCurrentTypes() const;

		[[nodiscard]] const ObjIdNameMap<TypeOfData>& getTodTypes() const;

	private:
		ObjIdNameMap<TypeOfData> tod_types;

		valid_type::ValidTypeMap types;
	};
}
