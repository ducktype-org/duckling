#pragma once

#include <variant>
#include <json/json.hpp>

namespace vm::api {
	struct ResumeError {};
	struct PauseError {};
	struct RunError {};
	struct JoinError {};
	struct OtherError {
		std::string error;
		JS_OBJ(error);
	};
	
	using CoreOperationErrorVariant = std::variant <
		ResumeError,
		PauseError,
		RunError,
		JoinError,
		OtherError,
		LoadProgramError
	>;

	struct CoreOperationError {
		CoreOperationErrorVariant error;
		JS_OBJ(error);
	};
}

REGISTER_PARSE_TYPE_ALIAS(vm::api::ResumeError, "ResumeError");
REGISTER_PARSE_TYPE_ALIAS(vm::api::PauseError, "PauseError");
REGISTER_PARSE_TYPE_ALIAS(vm::api::RunError, "RunError");
REGISTER_PARSE_TYPE_ALIAS(vm::api::JoinError, "JoinError");
REGISTER_PARSE_TYPE_ALIAS(vm::api::OtherError, "OtherError");
REGISTER_PARSE_TYPE_ALIAS(vm::api::CoreOperationError, "CoreOperationError");
