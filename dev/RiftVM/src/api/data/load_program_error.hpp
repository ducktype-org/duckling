#pragma once

#include <json/json.hpp>

namespace vm::api {
	struct LoadProgramError {
		std::string why;

		NLOHMANN_DEFINE_TYPE_INTRUSIVE(LoadProgramError, why);
	};
}

REGISTER_PARSE_TYPE_ALIAS(vm::api::LoadProgramError, "LoadProgramError")
