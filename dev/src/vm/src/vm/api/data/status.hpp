#pragma once

#include <json/json.hpp>

#include <variant>

namespace vm::api {

	using ExitCode = i64;

	struct Running {};

	struct Paused {};

	struct WaitingForInput {};

	struct NotStarted {};

	struct ExecutionCompleted {
		ExitCode exit_code;
		NLOHMANN_DEFINE_TYPE_INTRUSIVE(vm::api::ExecutionCompleted, exit_code);
	};

	struct ExecutionStopped {};

	struct ExecutionPanicked {
		std::string error_message;
		NLOHMANN_DEFINE_TYPE_INTRUSIVE(ExecutionPanicked, error_message);
	};

	struct ExecutionNotStarted {};

	struct Parsing {};

	struct TypeAnalysis {};

	using ProcStatus = std::variant<
		Running,
		Paused,
		WaitingForInput,
		NotStarted,
		ExecutionCompleted,
		ExecutionStopped,
		ExecutionPanicked,
		ExecutionNotStarted,
		Parsing,
		TypeAnalysis>;

	constexpr bool isStatusTerminal(const ProcStatus& status) {
		return std::holds_alternative<ExecutionCompleted>(status)
		    || std::holds_alternative<ExecutionStopped>(status)
		    || std::holds_alternative<ExecutionPanicked>(status);
	}

	constexpr bool executingStarted(const ProcStatus& status) {
		return !(
			std::holds_alternative<ExecutionNotStarted>(status)
			|| std::holds_alternative<Parsing>(status)
			|| std::holds_alternative<TypeAnalysis>(status)
		);
	}

	constexpr bool isExecuting(const ProcStatus& status) {
		return !isStatusTerminal(status) && executingStarted(status);
	}

}


JSON_REGISTER_TYPE_WITH_NAME(vm::api::ExecutionNotStarted, "ExecutionNotStarted")
JSON_REGISTER_TYPE_WITH_NAME(vm::api::Parsing, "Parsing")
JSON_REGISTER_TYPE_WITH_NAME(vm::api::TypeAnalysis, "TypeAnalysis")
JSON_REGISTER_TYPE_WITH_NAME(vm::api::WaitingForInput, "WaitingForInput")
JSON_REGISTER_TYPE_WITH_NAME(vm::api::NotStarted, "NotStarted")

JSON_REGISTER_TYPE_WITH_NAME(vm::api::Paused, "Paused")
JSON_REGISTER_TYPE_WITH_NAME(vm::api::Running, "Running")
JSON_REGISTER_TYPE_WITH_NAME(vm::api::ExecutionPanicked, "ExecutionPanicked")
JSON_REGISTER_TYPE_WITH_NAME(vm::api::ExecutionCompleted, "ExecutionCompleted")
JSON_REGISTER_TYPE_WITH_NAME(vm::api::ExecutionStopped, "ExecutionStopped")
