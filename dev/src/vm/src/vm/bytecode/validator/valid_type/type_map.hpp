// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <vm/bytecode/validator/valid_type/valid_type_id.hpp>
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
