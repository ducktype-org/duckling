#pragma once

#include <variant>
#include <json/json.hpp>
#include "load_program_error.hpp"

namespace vm::api {
	struct ResumeError {};

	struct PauseError {};

	struct RunError {};

	struct JoinError {};

	struct OtherError {
		std::string error;
		NLOHMANN_DEFINE_TYPE_INTRUSIVE(OtherError, error);
	};

	using CoreOperationErrorVariant
		= std::variant<ResumeError, PauseError, RunError, JoinError, OtherError, LoadProgramError>;

	struct CoreOperationError {
		CoreOperationErrorVariant error;
		NLOHMANN_DEFINE_TYPE_INTRUSIVE(CoreOperationError, error);
	};
}

REGISTER_PARSE_TYPE_ALIAS(vm::api::ResumeError, "ResumeError");
REGISTER_PARSE_TYPE_ALIAS(vm::api::PauseError, "PauseError");
REGISTER_PARSE_TYPE_ALIAS(vm::api::RunError, "RunError");
REGISTER_PARSE_TYPE_ALIAS(vm::api::JoinError, "JoinError");
REGISTER_PARSE_TYPE_ALIAS(vm::api::OtherError, "OtherError");
REGISTER_PARSE_TYPE_ALIAS(vm::api::CoreOperationError, "CoreOperationError");
