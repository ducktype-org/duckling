#pragma once

#include <vm/core/thread/vmvalue.hpp>

#include <json/json.hpp>

#include <variant>

namespace vm::api {
	struct Running {};

	struct Paused {};

	struct Sleeping {};

	struct NotStarted {};

	using ExitValue = Ref<VmValue>;

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
		return std::holds_alternative<ExecutionCompleted>(status)
		    || std::holds_alternative<ExecutionStopped>(status)
		    || std::holds_alternative<ExecutionPanicked>(status);
	}

	constexpr bool hasExecutionStarted(const ProcStatus& status) {
		return !(std::holds_alternative<NotStarted>(status));
	}

	constexpr bool isExecuting(const ProcStatus& status) {
		// @note This function is equivalent to the following:
		// return !isStatusTerminal(status) && hasExecutionStarted(status);
		return std::holds_alternative<Running>(status) || std::holds_alternative<Paused>(status)
		    || std::holds_alternative<Sleeping>(status);
	}

	constexpr bool canRespond(const ProcStatus& status) {
		return std::holds_alternative<NotStarted>(status) || std::holds_alternative<Paused>(status)
		    || isStatusTerminal(status);
	}
}


JSON_REGISTER_TYPE_WITH_NAME(vm::api::Sleeping, "WaitingForInput")
JSON_REGISTER_TYPE_WITH_NAME(vm::api::NotStarted, "NotStarted")

JSON_REGISTER_TYPE_WITH_NAME(vm::api::Paused, "Paused")
JSON_REGISTER_TYPE_WITH_NAME(vm::api::Running, "Running")
JSON_REGISTER_TYPE_WITH_NAME(vm::api::ExecutionPanicked, "ExecutionPanicked")
JSON_REGISTER_TYPE_WITH_NAME(vm::api::ExecutionCompleted, "ExecutionCompleted")
JSON_REGISTER_TYPE_WITH_NAME(vm::api::ExecutionStopped, "ExecutionStopped")
