#pragma once

#include "load_program_error.hpp"

#include <json/json.hpp>
#include <variant>

namespace vm::api {
	struct ResumeError {};

	struct PauseError {};

	struct RunError {};

	struct JoinError {};

	struct AttachDetachError {};

	struct OtherError {
		std::string error;
		NLOHMANN_DEFINE_TYPE_INTRUSIVE(OtherError, error);
	};

	using CoreOperationError = std::variant<
		ResumeError,
		PauseError,
		RunError,
		JoinError,
		AttachDetachError,
		OtherError,
		LoadProgramError>;
}

JSON_REGISTER_TYPE_WITH_NAME(vm::api::ResumeError, "ResumeError");
JSON_REGISTER_TYPE_WITH_NAME(vm::api::PauseError, "PauseError");
JSON_REGISTER_TYPE_WITH_NAME(vm::api::RunError, "RunError");
JSON_REGISTER_TYPE_WITH_NAME(vm::api::JoinError, "JoinError");
JSON_REGISTER_TYPE_WITH_NAME(vm::api::OtherError, "OtherError");
JSON_REGISTER_TYPE_WITH_NAME(vm::api::AttachDetachError, "AttachDetachError");
