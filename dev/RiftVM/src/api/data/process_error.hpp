#pragma once

#include "load_program_error.hpp"
#include <json/json.hpp>
#include <variant>

namespace vm::api {
	struct ProcessNotFound {};

	using ProcessError = std::variant<ProcessNotFound>;
}

REGISTER_PARSE_TYPE_ALIAS(vm::api::ProcessNotFound, "ProcessNotFound")
