#pragma once

#include <base/extend_cpp/variant_match.hpp>

#include <vm/core/vmvalue/vmvalue.hpp>

#include <json/json.hpp>

#include <variant>

namespace vm::api {
	struct Running {};

	struct Paused {};

	struct Sleeping {};

	struct NotStarted {};

	using ExitValue = std::vector<Ref<VmValue>>;

	struct ExecutionCompleted {
		ExitValue exit_value;
		NLOHMANN_DEFINE_TYPE_INTRUSIVE(ExecutionCompleted, exit_value);
	};

	struct ExecutionStopping {};

	struct ExecutionStopped {};

	struct ExecutionPanicked {
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
}


JSON_REGISTER_TYPE_WITH_NAME(vm::api::Sleeping, "Sleeping")
JSON_REGISTER_TYPE_WITH_NAME(vm::api::NotStarted, "NotStarted")

JSON_REGISTER_TYPE_WITH_NAME(vm::api::Paused, "Paused")
JSON_REGISTER_TYPE_WITH_NAME(vm::api::Running, "Running")
JSON_REGISTER_TYPE_WITH_NAME(vm::api::ExecutionStopping, "ExecutionStopping")
JSON_REGISTER_TYPE_WITH_NAME(vm::api::ExecutionPanicked, "ExecutionPanicked")
JSON_REGISTER_TYPE_WITH_NAME(vm::api::ExecutionCompleted, "ExecutionCompleted")
JSON_REGISTER_TYPE_WITH_NAME(vm::api::ExecutionStopped, "ExecutionStopped")
