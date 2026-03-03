#pragma once

#include <vm/bytecode/validator/type/type_id.hpp>
#include <vm/utils/stable_obj_id_name_map.hpp>

namespace vm::code::valid_type {
	class ValidType;

	/**
	 * @brief A map of types. It maps TypeID to Type. It is used to store types
	 * in e.g. the TypeContext.
	 * @TODO: #1306 Change this to StableObjIdNameMap<Type>
	 */
	using ValidTypeMap = ObjIdNameMap<ValidType, ValidTypeID>;

}
