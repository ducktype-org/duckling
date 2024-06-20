#pragma once

#include <variant>
#include <json/json.hpp>

namespace vm::api {
	struct ExecutionNotStarted {};

	struct Parsing {};

	struct TypeAnalysis {};

	struct Panicked {
		std::exception exception;
		NLOHMANN_DEFINE_TYPE_INTRUSIVE(Panicked, exception);
	};

	struct Paused {};

	struct Running {};

	struct PausedOnError {
		std::string reason;
		NLOHMANN_DEFINE_TYPE_INTRUSIVE(PausedOnError, reason);
	};

	struct WaitingForInput {};

	struct NotStarted {};

	using ExecStatus
		= std::variant<Panicked, Running, Paused, PausedOnError, WaitingForInput, NotStarted>;

	struct Executing {
		ExecStatus exec_status;
		NLOHMANN_DEFINE_TYPE_INTRUSIVE(Executing, exec_status);
	};

	using VCPUStatus
		= std::variant<ExecutionNotStarted, Parsing, TypeAnalysis, Panicked, Executing>;
}

REGISTER_PARSE_TYPE_ALIAS(vm::api::ExecutionNotStarted, "ExecutionNotStarted")
REGISTER_PARSE_TYPE_ALIAS(vm::api::Parsing, "Parsing")
REGISTER_PARSE_TYPE_ALIAS(vm::api::TypeAnalysis, "TypeAnalysis")
REGISTER_PARSE_TYPE_ALIAS(vm::api::Executing, "Executing")
REGISTER_PARSE_TYPE_ALIAS(vm::api::WaitingForInput, "WaitingForInput")
REGISTER_PARSE_TYPE_ALIAS(vm::api::NotStarted, "NotStarted")

REGISTER_PARSE_TYPE_ALIAS(vm::api::Paused, "Paused")
REGISTER_PARSE_TYPE_ALIAS(vm::api::Panicked, "Panicked")
REGISTER_PARSE_TYPE_ALIAS(vm::api::Running, "Running")
REGISTER_PARSE_TYPE_ALIAS(vm::api::PausedOnError, "PausedOnError")
