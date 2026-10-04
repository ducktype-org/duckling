#pragma once

#include <base/extend_cpp/strongly_typed_id.hpp>
#include <base/types/ints.hpp>

namespace vm::api {
	/**
	 * @brief VMThread's ID.
	 */
	STRONG_TYPEDEF_ID_DIRECT_CREATION(ThreadID);

	/**
	 * @brief The ID of a process's main VMThread.
	 */
	inline constexpr ThreadID MAIN_THREAD_ID{ 0 };
}

ID_STD_HASH(vm::api::ThreadID);
