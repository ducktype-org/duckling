#pragma once

#include <string>

// Since C++ doesn't have an error-propagating operator, this macro
// essentially implements it - checks if `value` has an error and if it does, then
// returns an error as well, otherwise stores an unpacked value
// inside a new variable named `name`.
// For interested: https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2022/p2561r1.html#ref-P2561R0
#define IF_ERR_RET_ELSE_VALUE(value, name)                         \
	if (!value.has_value()) return std::unexpected(value.error()); \
	auto&& name = *value;

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
