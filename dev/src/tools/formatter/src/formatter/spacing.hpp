/**
 * @file spacing.hpp
 * @brief The inter-token spacing oracle of the formatter.
 */
#pragma once

#include "config.hpp"

#include <lexer/token.hpp>

namespace formatter {

	/**
	 * @brief Decides whether a single space separates @p prev from @p cur in inline context.
	 *
	 * @param prev The previously rendered significant token, or nullptr at the start of a line,
	 *             a bracket group, or an exploded group element.
	 * @param prev2 The token before @p prev, where known. Only a few rules need it, such as
	 *              telling the custom-operator name in `fun +*(a, b)` from a binary operator.
	 */
	[[nodiscard]]
	bool needSpace(
		const FormatConfig& config,
		const lexer::Token* prev,
		const lexer::Token& cur,
		const lexer::Token* prev2 = nullptr
	);
}
