#pragma once

#include <variant>
#include <json/json.hpp>

namespace vm::api {
	struct ExecutionNotStarted {};

	struct Parsing {};

	struct TypeAnalysis {};

	struct ExecutionPanicked {
		std::exception exception;
		NLOHMANN_DEFINE_TYPE_INTRUSIVE(ExecutionPanicked, exception);
	};

	struct Paused {};

	struct Running {};

	struct PausedOnError {
		std::string reason;
		NLOHMANN_DEFINE_TYPE_INTRUSIVE(PausedOnError, reason);
	};

	struct WaitingForInput {};

	struct NotStarted {};

	struct ExecutionCompleted {};

	struct ExecutionStopped {};

	using ExecStatus = std::variant<
		Running,
		Paused,
		PausedOnError,
		WaitingForInput,
		NotStarted,
		ExecutionCompleted,
		ExecutionStopped,
		ExecutionPanicked>;

	struct Executing {
		ExecStatus exec_status;
		NLOHMANN_DEFINE_TYPE_INTRUSIVE(vm::api::Executing, exec_status);
	};

	using ProcStatus = std::variant<ExecutionNotStarted, Parsing, TypeAnalysis, Executing>;
}


JSON_REGISTER_TYPE_WITH_NAME(vm::api::ExecutionNotStarted, "ExecutionNotStarted")
JSON_REGISTER_TYPE_WITH_NAME(vm::api::Parsing, "Parsing")
JSON_REGISTER_TYPE_WITH_NAME(vm::api::TypeAnalysis, "TypeAnalysis")
JSON_REGISTER_TYPE_WITH_NAME(vm::api::WaitingForInput, "WaitingForInput")
JSON_REGISTER_TYPE_WITH_NAME(vm::api::NotStarted, "NotStarted")

JSON_REGISTER_TYPE_WITH_NAME(vm::api::Paused, "Paused")
JSON_REGISTER_TYPE_WITH_NAME(vm::api::Running, "Running")
JSON_REGISTER_TYPE_WITH_NAME(vm::api::Executing, "Executing")
JSON_REGISTER_TYPE_WITH_NAME(vm::api::ExecutionPanicked, "ExecutionPanicked")
JSON_REGISTER_TYPE_WITH_NAME(vm::api::PausedOnError, "PausedOnError")
JSON_REGISTER_TYPE_WITH_NAME(vm::api::ExecutionCompleted, "ExecutionCompleted")
JSON_REGISTER_TYPE_WITH_NAME(vm::api::ExecutionStopped, "ExecutionStopped")
