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

JSON_REGISTER_TYPE_WITH_NAME(vm::api::ResumeError, "ResumeError");
JSON_REGISTER_TYPE_WITH_NAME(vm::api::PauseError, "PauseError");
JSON_REGISTER_TYPE_WITH_NAME(vm::api::RunError, "RunError");
JSON_REGISTER_TYPE_WITH_NAME(vm::api::JoinError, "JoinError");
JSON_REGISTER_TYPE_WITH_NAME(vm::api::OtherError, "OtherError");
JSON_REGISTER_TYPE_WITH_NAME(vm::api::CoreOperationError, "CoreOperationError");
