#pragma once

#include <string>
#include <base/define_helper.hpp>
#include <variant>
#include "helios_result.hpp"

// This is a unique variable per macro - assuming every macro is in a separate line.
#define RES_VAR_NAME CONCAT_2(result_storage_aBz4vq2_, __LINE__)

// Since C++ doesn't have an error-propagating operator, this macro
// essentially implements it - checks if `value` has an error and if it does, then
// returns an error as well, otherwise stores an unpacked value
// inside a new variable named `name`.
// For interested: https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2022/p2561r1.html#ref-P2561R0
#define UNPACK_RESULT(new_value, name)      UNPACK_RESULT_CUSTOM(new_value, auto&& name)
#define UNPACK_RESULT_COPY(new_value, name) UNPACK_RESULT_CUSTOM(new_value, auto name)

#define UNPACK_RESULT_CUSTOM(new_value, name)                               \
	auto&& RES_VAR_NAME = new_value;                                        \
	if (!RES_VAR_NAME.has_value())                                          \
		return compiler::helios::errors::HUnexpected(RES_VAR_NAME.error()); \
	name = RES_VAR_NAME.value()

namespace compiler::helios::errors {
	struct SymbolNotFoundError {};

	struct AmbiguityError {};

	struct ExpressionParsingError {};
}
