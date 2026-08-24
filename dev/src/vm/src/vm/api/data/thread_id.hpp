#pragma once

#include <base/extend_cpp/strongly_typed_id.hpp>
#include <base/types/ints.hpp>

namespace vm::api {
	/**
	 * @brief VMThread's ID.
	 */
	STRONG_TYPEDEF_ID_DIRECT_CREATION(ThreadID);
}

ID_STD_HASH(vm::api::ThreadID);
