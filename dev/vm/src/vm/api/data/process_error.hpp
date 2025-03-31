#pragma once

#include <variant>

#include "load_program_error.hpp"
#include <json/json.hpp>

namespace vm::api {
	struct ProcessNotFound {};

	using ProcessError = std::variant<ProcessNotFound>;
}

JSON_REGISTER_TYPE_WITH_NAME(vm::api::ProcessNotFound, "ProcessNotFound")
