#pragma once

#include <variant>
#include <json/json.hpp>
#include "load_program_error.hpp"

namespace vm::api {
	struct ProcessNotFound {};

	using ProcessError = std::variant<ProcessNotFound>;
}

JSON_REGISTER_TYPE_WITH_NAME(vm::api::ProcessNotFound, "ProcessNotFound")
