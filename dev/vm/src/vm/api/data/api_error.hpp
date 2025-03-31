#pragma once

#include <variant>

#include "core_operation_error.hpp"
#include "process_error.hpp"

namespace vm::api {
	struct WrongResponse {};

	using ApiError = std::variant<ProcessError, CoreOperationError, WrongResponse>;
}

JSON_REGISTER_TYPE_WITH_NAME(vm::api::WrongResponse, "WrongResponse")
