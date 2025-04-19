#pragma once

#include <json/json.hpp>

namespace vm::api {
	/**
	 * @brief Error for reporting when current state is incapable of producing a proper result.
	 */
	struct StateError {
		std::string why;

		NLOHMANN_DEFINE_TYPE_INTRUSIVE(StateError, why);
	};
}

JSON_REGISTER_TYPE_WITH_NAME(vm::api::StateError, "StateError")
