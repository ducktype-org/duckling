#pragma once

#include "core_operation_error.hpp"
#include "process_error.hpp"

#include <variant>

namespace vm::api {
	struct WrongResponse {};

	using ApiError = std::variant<ProcessError, CoreOperationError, WrongResponse>;
}

JS_EMPTY(vm::api::WrongResponse)
REGISTER_PARSE_TYPE_ALIAS(vm::api::WrongResponse, "WrongResponse")
