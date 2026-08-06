#pragma once

#include <base/extend_cpp/variant_match.hpp>

#include <vm/core/vmvalue/ivmvalue.hpp>

#include <json/json.hpp>

#include <variant>

namespace vm::api {
	struct Running {};

	struct Paused {};

	struct Sleeping {};

	struct NotStarted {};

	// @TODO: #2720 Change it back to std::vector
	using ExitValue = std::variant<i64, std::vector<Ref<IVMValue>>>;

	struct ExecutionCompleted {
		ExitValue exit_value;
		NLOHMANN_DEFINE_TYPE_INTRUSIVE(ExecutionCompleted, exit_value);
	};

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
		return v_matches(status, Running, Paused, Sleeping);
	}

	constexpr bool canRespond(const ProcStatus& status) {
		return v_matches(status, NotStarted, Paused) || isStatusTerminal(status);
	}
}


JSON_REGISTER_TYPE_WITH_NAME(vm::api::Sleeping, "Sleeping")
JSON_REGISTER_TYPE_WITH_NAME(vm::api::NotStarted, "NotStarted")

JSON_REGISTER_TYPE_WITH_NAME(vm::api::Paused, "Paused")
JSON_REGISTER_TYPE_WITH_NAME(vm::api::Running, "Running")
JSON_REGISTER_TYPE_WITH_NAME(vm::api::ExecutionPanicked, "ExecutionPanicked")
JSON_REGISTER_TYPE_WITH_NAME(vm::api::ExecutionCompleted, "ExecutionCompleted")
JSON_REGISTER_TYPE_WITH_NAME(vm::api::ExecutionStopped, "ExecutionStopped")
