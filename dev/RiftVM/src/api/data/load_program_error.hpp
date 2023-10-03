#pragma once

#include <json/json.hpp>

namespace vm::api {
	struct LoadProgramError {
		std::string why;

		JS_OBJ(why);
	};
}  // namespace vm::api

REGISTER_PARSE_TYPE_ALIAS(vm::api::LoadProgramError, "LoadProgramError")
