// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <base/extend_cpp/variant_match.hpp>

#include <vm/core/vmvalue/ivmvalue.hpp>

#include <json/json.hpp>

#include <string_view>
#include <type_traits>
#include <variant>

namespace vm::api {
	struct Running final {};

	struct Paused final {};

	struct Sleeping final {};

	struct NotStarted final {};

	// @TODO: #2720 Change it back to std::vector
	using ExitValue = std::variant<i64, std::vector<Ref<IVMValue>>>;

	struct ExecutionCompleted final {
		ExitValue exit_value;
		NLOHMANN_DEFINE_TYPE_INTRUSIVE(ExecutionCompleted, exit_value);
	};

	struct ExecutionStopping final {};

	struct ExecutionStopped final {};

	struct ExecutionPanicked final {
		std::string error_message;
		NLOHMANN_DEFINE_TYPE_INTRUSIVE(ExecutionPanicked, error_message);
	};

	using ProcStatus = std::variant<
		NotStarted,
		Running,
		Paused,
		Sleeping,
		ExecutionStopping,
		ExecutionCompleted,
		ExecutionStopped,
		ExecutionPanicked>;

	/**
	 * @note When changing the behaviour of these function remember to change ones in
	 * `process_state.hpp` as well.
	 */
	constexpr bool isStatusTerminal(const ProcStatus& status) {
		return v_matches(status, ExecutionCompleted, ExecutionStopped, ExecutionPanicked);
	}

	constexpr bool hasExecutionStarted(const ProcStatus& status) {
		return !v_matches(status, NotStarted);
	}

	constexpr bool isExecuting(const ProcStatus& status) {
		// @note This function is equivalent to the following:
		// return !isStatusTerminal(status) && hasExecutionStarted(status);
		return v_matches(status, Running, Paused, Sleeping, ExecutionStopping);
	}

	constexpr bool canRespond(const ProcStatus& status) {
		return v_matches(status, NotStarted, Paused) || isStatusTerminal(status);
	}

	constexpr bool canDeinit(const ProcStatus& status) {
		return v_matches(status, NotStarted, ExecutionCompleted);
	}
}


JSON_REGISTER_TYPE_WITH_NAME(vm::api::Sleeping, "Sleeping")
JSON_REGISTER_TYPE_WITH_NAME(vm::api::NotStarted, "NotStarted")

JSON_REGISTER_TYPE_WITH_NAME(vm::api::Paused, "Paused")
JSON_REGISTER_TYPE_WITH_NAME(vm::api::Running, "Running")
JSON_REGISTER_TYPE_WITH_NAME(vm::api::ExecutionStopping, "ExecutionStopping")
JSON_REGISTER_TYPE_WITH_NAME(vm::api::ExecutionPanicked, "ExecutionPanicked")
JSON_REGISTER_TYPE_WITH_NAME(vm::api::ExecutionCompleted, "ExecutionCompleted")
JSON_REGISTER_TYPE_WITH_NAME(vm::api::ExecutionStopped, "ExecutionStopped")

namespace vm::api {
	/**
	 * @brief Name of the held `ProcStatus` alternative
	 */
	[[nodiscard]] inline std::string_view statusName(const ProcStatus& status) {
		return VISIT(status, held, return js::typeName<std::remove_cvref_t<decltype(held)>>());
	}
}
