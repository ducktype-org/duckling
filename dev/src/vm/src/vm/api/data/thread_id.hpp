#pragma once

#include <base/extend_cpp/strongly_typed_int.hpp>
#include <base/types/ints.hpp>

namespace vm::api {
	/**
	 * @brief VMThread's ID.
	 */
	STRONG_TYPEDEF_INT(ThreadID, u64);
}
