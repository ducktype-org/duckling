#pragma once

#include <string>

// This is a unique variable per macro - assuming every macro is in a separate line.
#define RES_VAR_NAME CONCAT_2(result_storage_, __LINE__)

// Since C++ doesn't have an error-propagating operator, this macro
// essentially implements it - checks if `value` has an error and if it does, then
// returns an error as well, otherwise stores an unpacked value
// inside a new variable named `name`.
// For interested: https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2022/p2561r1.html#ref-P2561R0
#define UNPACK_RESULT(value, name)     UNPACK_RESULT_CUSTOM(value, auto&& name)
#define UNPACK_RESULT_MUT(value, name) UNPACK_RESULT_CUSTOM(value, auto name)

#define UNPACK_RESULT_CUSTOM(value, name)                                        \
	auto&& RES_VAR_NAME = value;                                                 \
	if (!RES_VAR_NAME.has_value()) return std::unexpected(RES_VAR_NAME.error()); \
	name = *RES_VAR_NAME

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

	struct ExpressionParsingError {
		std::string message;
	};
}

#define HELIOS_ASSERT(cnd, ErrTp, ...) \
	if (!(cnd)) return std::unexpected(ErrTp(__VA_ARGS__))

#define HELIOS_PANIC(ErrTp, ...) return std::unexpected(ErrTp(__VA_ARGS__))
