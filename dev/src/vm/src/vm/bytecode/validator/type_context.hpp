#pragma once

#include "type.hpp"

#include <vm/bytecode/bytecode.hpp>
#include <vm/bytecode/type_of_data.hpp>
#include <vm/utils/stable_obj_id_name_map.hpp>

namespace vm::code {
	/**
	 * @brief A simple container for types which doesn't allow duplicates. This structure is the
	 * main entry point for type verification and building.
	 */
	class TypeContext final {
	public:
		/**
		 * @brief Inserts new types.
		 * If a type is duplicated throws DuplicatedTypeError.
		 * If inserting new types would invalidate type context provided with function_signatures,
		 * throw an error.
		 */
		void insertAndValidate(
			const std::vector<TypeOfData>&                   types,
			const base::HashMap<base::StrID, FuncSignature>& function_signatures
		);

		const ObjIdNameMap<TypeOfData>& getCurrentTypes() const;

	private:
		ObjIdNameMap<TypeOfData> pod_types;

		// @TODO: #1306 Changes this to StableObjIdNameMap<Type>
		ObjIdNameMap<type::Type> types;
	};
}
