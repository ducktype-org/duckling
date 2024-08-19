#pragma once

#include <string>

// Some simple helpers
#define IF_ERR_GET_RET_ELSE_VALUE(value, name) if(!value.has_value()) return std::unexpected(value.error()); auto &&name = *value;

namespace compiler::helios::errors {
	/**
	 * @brief UnknownError is meant as a simple placeholder until a better name comes up.
	 */
	struct UnknownError {
		std::string message;
	};

	struct SymbolNotFoundError {};

	struct AmbiguityError {
		std::string message;
	};

	struct ExpressionParsingError {};
}
