#pragma once

#include <json/json.hpp>

#include <string>
#include <variant>

namespace vm::api {
	struct ResumeError {};

	struct PauseError {};

	struct RunError {
		std::string error;
		NLOHMANN_DEFINE_TYPE_INTRUSIVE(RunError, error);
	};

	struct JoinError {};

	struct AttachDetachError {};

	struct OtherError {
		std::string error;
		NLOHMANN_DEFINE_TYPE_INTRUSIVE(OtherError, error);
	};

	struct IOError {
		std::string error;
		NLOHMANN_DEFINE_TYPE_INTRUSIVE(IOError, error);
	};

	struct LoadProgramError {
		std::string why;

		NLOHMANN_DEFINE_TYPE_INTRUSIVE(LoadProgramError, why);
	};

	struct ProcessNotFound {};

	struct WrongResponse {};

	struct StateError {
		std::string why;

		NLOHMANN_DEFINE_TYPE_INTRUSIVE(StateError, why);
	};

	/**
	 * @brief Represents an error indicating that a feature is not yet implemented.
	 * @note After having received this error, the process is in undefined state, so it should be
	 * killed.
	 */
	struct NotImplementedError {
		std::string why;
		NLOHMANN_DEFINE_TYPE_INTRUSIVE(NotImplementedError, why);
	};

	using ApiError = std::variant<
		ResumeError,
		PauseError,
		RunError,
		JoinError,
		AttachDetachError,
		OtherError,
		IOError,
		LoadProgramError,
		ProcessNotFound,
		WrongResponse,
		StateError,
		NotImplementedError>;

	/**
	 * @brief Converts the ApiError to a string representation in a JSON format.
	 */
	std::string errorToString(const ApiError& api_error);

	/**
	 * @brief Convert ApiError to an errno-like integer code (POSIX errno values).
	 *
	 * Maps API error variants to reasonable errno values so callers can return
	 * the numeric error code instead of throwing exceptions.
	 */
	int errorToErrno(const ApiError& api_error);
}

JSON_REGISTER_TYPE_WITH_NAME(vm::api::ResumeError, "ResumeError");
JSON_REGISTER_TYPE_WITH_NAME(vm::api::PauseError, "PauseError");
JSON_REGISTER_TYPE_WITH_NAME(vm::api::RunError, "RunError");
JSON_REGISTER_TYPE_WITH_NAME(vm::api::JoinError, "JoinError");
JSON_REGISTER_TYPE_WITH_NAME(vm::api::AttachDetachError, "AttachDetachError");
JSON_REGISTER_TYPE_WITH_NAME(vm::api::OtherError, "OtherError");
JSON_REGISTER_TYPE_WITH_NAME(vm::api::IOError, "IOError");
JSON_REGISTER_TYPE_WITH_NAME(vm::api::LoadProgramError, "LoadProgramError");
JSON_REGISTER_TYPE_WITH_NAME(vm::api::ProcessNotFound, "ProcessNotFound");
JSON_REGISTER_TYPE_WITH_NAME(vm::api::WrongResponse, "WrongResponse");
JSON_REGISTER_TYPE_WITH_NAME(vm::api::StateError, "StateError");
JSON_REGISTER_TYPE_WITH_NAME(vm::api::NotImplementedError, "NotImplementedError");
