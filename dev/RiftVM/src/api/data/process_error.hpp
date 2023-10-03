#pragma once

#include <json/json.hpp>
#include <variant>

#include "load_program_error.hpp"

namespace vm::api {
	struct ProcessNotFound {};

	using ProcessError = std::variant<ProcessNotFound>;
}  // namespace vm::api

REGISTER_PARSE_TYPE_ALIAS(vm::api::ProcessNotFound, "ProcessNotFound")
