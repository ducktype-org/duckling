#pragma once

#include "core_operation_error.hpp"
#include "process_error.hpp"
#include "state_error.hpp"

#include <variant>

namespace vm::api {
	struct WrongResponse {};

	using ApiError = std::variant<ProcessError, CoreOperationError, WrongResponse, StateError>;

	std::string errorToString(const ApiError& api_error);
}

JSON_REGISTER_TYPE_WITH_NAME(vm::api::WrongResponse, "WrongResponse")
