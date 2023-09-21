#pragma once

#include <json/json.hpp>

namespace vm::api {
	struct LoadProgramError {
		std::string why;
		
		JS_OBJ(why);
	};
}

REGISTER_PARSE_TYPE_ALIAS(vm::api::LoadProgramError, "LoadProgramError")
