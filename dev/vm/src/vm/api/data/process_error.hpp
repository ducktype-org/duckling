#pragma once

#include "load_program_error.hpp"

#include <json/json.hpp>
#include <variant>

namespace vm::api {
	struct ProcessNotFound {};

	using ProcessError = std::variant<ProcessNotFound>;
}

JSON_REGISTER_TYPE_WITH_NAME(vm::api::ProcessNotFound, "ProcessNotFound")
