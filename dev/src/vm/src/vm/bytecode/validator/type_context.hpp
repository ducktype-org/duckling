#pragma once

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
		 * @brief Inserts a new type. If a type is duplicated throws DuplicatedTypeError.
		 */
		void insertType(const TypeOfData& type);

		const ObjIdNameMap<TypeOfData>& getCurrentTypes() const;

	private:
		ObjIdNameMap<TypeOfData> types;
	};
}
