#pragma once

#include <variant>
#include "process_error.hpp"
#include "core_operation_error.hpp"

namespace vm::api {
	struct WrongResponse {};

	using ApiError = std::variant<
		ProcessError,
		CoreOperationError,
		WrongResponse
	>;
}

JS_EMPTY(vm::api::WrongResponse)
REGISTER_PARSE_TYPE_ALIAS(vm::api::WrongResponse, "WrongResponse")
