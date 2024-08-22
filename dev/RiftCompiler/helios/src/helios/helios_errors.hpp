#pragma once

#include <string>

// Since C++ doesn't have an error-propagating operator, this macro
// essentially implements it - checks if `value` has an error and if it does, then
// returns an error as well, otherwise stores an unpacked value
// inside a new variable named `name`.
// For interested: https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2022/p2561r1.html#ref-P2561R0
#define UNPACK_RESULT(value, name)                                         \
	auto&& CONCAT_2(result_storage, __LINE__) = value;                     \
	if (auto&& val = CONCAT_2(result_storage, __LINE__); !val.has_value()) \
		return std::unexpected(val.error());                               \
	auto&& name = *CONCAT_2(result_storage, __LINE__)

#define UNPACK_RESULT_MUT(value, name)                                     \
	auto&& CONCAT_2(result_storage, __LINE__) = value;                     \
	if (auto&& val = CONCAT_2(result_storage, __LINE__); !val.has_value()) \
		return std::unexpected(val.error());                               \
	auto name = *CONCAT_2(result_storage, __LINE__)

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
