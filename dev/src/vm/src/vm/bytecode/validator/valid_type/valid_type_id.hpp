#pragma once

#include <base/extend_cpp/strongly_typed_id.hpp>

namespace vm::code::valid_type {
	/**
	 * @brief TypeID is a unique identifier of a type. It is used to refer to types. It is an index
	 * in the TypeContext's type map.
	 * @note It's done this way to allow copying of types without worrying about pointer/reference
	 * references to types can be invalidated when the TypeContext is e.g. copied.
	 * @TODO: #1306 Maybe it can become Ref<Type>?
	 */
	STRONG_TYPEDEF_ID_DIRECT_CREATION(ValidTypeID);

}

ID_STD_HASH(vm::code::valid_type::ValidTypeID);
