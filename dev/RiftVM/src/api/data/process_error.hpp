#pragma once

#include <variant>
#include <json/json.hpp>
#include "load_program_error.hpp"

namespace vm::api {
	struct ProcessNotFound{};
	
	using ProcessError = std::variant<
		ProcessNotFound
	>;
}

REGISTER_PARSE_TYPE_ALIAS(vm::api::ProcessNotFound, "ProcessNotFound")
