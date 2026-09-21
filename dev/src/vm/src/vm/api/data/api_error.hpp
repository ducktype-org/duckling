#pragma once

#include <events/emitter.hpp>

#include <base/pointers/ref.hpp>
#include <base/pointers/shared_box.hpp>

#include <vm/core/safe/vmvalue/safe_vmvalue.hpp>

#include <json/json.hpp>

#include <string>
#include <variant>
#include <vector>

namespace vm::api {
	struct ResumeError final {
		std::string why;
		NLOHMANN_DEFINE_TYPE_INTRUSIVE(ResumeError, why);
	};

	struct PauseError final {
		std::string why;
		NLOHMANN_DEFINE_TYPE_INTRUSIVE(PauseError, why);
	};

	struct RunError final {
		std::string error;
		NLOHMANN_DEFINE_TYPE_INTRUSIVE(RunError, error);
	};

	struct JoinError final {};

	struct AttachDetachError final {};

	struct OtherError final {
		std::string error;
		NLOHMANN_DEFINE_TYPE_INTRUSIVE(OtherError, error);
	};

	struct IOError final {
		std::string error;
		NLOHMANN_DEFINE_TYPE_INTRUSIVE(IOError, error);
	};

	struct LoadProgramError final {
		std::string why;

		NLOHMANN_DEFINE_TYPE_INTRUSIVE(LoadProgramError, why);
	};

	struct ProcessNotFound final {};

	struct WrongResponse final {};

	struct StateError final {
		std::string why;

		NLOHMANN_DEFINE_TYPE_INTRUSIVE(StateError, why);
	};

	/**
	 * @brief A computation which run panicked. Returned by every endpoint executing bytecode when
	 * an evaluation fails.
	 */
	struct Panicked final {
		std::string why;

		NLOHMANN_DEFINE_TYPE_INTRUSIVE(Panicked, why);
	};

	struct UnsupportedOperation final {
		std::string why;
		NLOHMANN_DEFINE_TYPE_INTRUSIVE(UnsupportedOperation, why);
	};

	struct IncompleteExprEval final {
		SharedBox<events::Emitter<std::vector<Ref<SafeVMValue>>>> val;
		std::string                                               why;
		NLOHMANN_DEFINE_TYPE_INTRUSIVE(IncompleteExprEval, why);
	};

	/**
	 * @brief Represents an error indicating that a feature is not yet implemented.
	 */
	struct NotImplementedError final {
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
		Panicked,
		NotImplementedError,
		IncompleteExprEval,
		UnsupportedOperation>;

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
JSON_REGISTER_TYPE_WITH_NAME(vm::api::Panicked, "Panicked");
JSON_REGISTER_TYPE_WITH_NAME(vm::api::NotImplementedError, "NotImplementedError");
JSON_REGISTER_TYPE_WITH_NAME(vm::api::UnsupportedOperation, "UnsupportedOperation");
JSON_REGISTER_TYPE_WITH_NAME(vm::api::IncompleteExprEval, "IncompleteExprEval");
