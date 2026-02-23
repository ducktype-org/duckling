#pragma once

#include "type.hpp"

#include <base/except/exceptions.hpp>

#include <vm/bytecode/bytecode.hpp>
#include <vm/bytecode/type_of_data.hpp>
#include <vm/utils/stable_obj_id_name_map.hpp>

namespace vm::code {
	// @TODO: #1306 Change this to StableObjIdNameMap<Type>
	using TypeMap = ObjIdNameMap<type::Type>;

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
		 * @note After an exception is thrown, the TypeContext's state is undefined.
		 */
		void insertAndValidate(
			const std::vector<TypeOfData>&                   new_types,
			const base::HashMap<base::StrID, FuncSignature>& function_signatures
		);

		const TypeMap& getCurrentTypes() const;

		const ObjIdNameMap<TypeOfData>& getPodTypes() const;

	private:
		ObjIdNameMap<TypeOfData> pod_types;

		TypeMap types;
	};
}
