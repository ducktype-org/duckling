#pragma once

#include <json/json.hpp>

namespace vm::api {
	struct LoadProgramError {
		std::string why;

		NLOHMANN_DEFINE_TYPE_INTRUSIVE(LoadProgramError, why);
	};
}

JSON_REGISTER_TYPE_WITH_NAME(vm::api::LoadProgramError, "LoadProgramError")
