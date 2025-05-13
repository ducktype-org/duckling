#include "api_error.hpp"

namespace vm::api {
	std::string errorToString(const ApiError& api_error) {
        return nlohmann::to_string(nlohmann::json(api_error));
    }
}
