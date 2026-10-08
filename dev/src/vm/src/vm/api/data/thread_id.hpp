// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

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
