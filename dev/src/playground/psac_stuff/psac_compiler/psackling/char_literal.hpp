#pragma once

#include <frontend/pst_parser/elements/hierarchy/expressions/char_value.hpp>
#include <frontend/pst_parser/elements/includes/basic.hpp>  // IWYU pragma: keep

#include <base/str/str_utils.hpp>

#include <stdexcept>

namespace pg {
	/// Decodes a PST char literal (`'a'`, `'\n'`, ...) to its byte value. The PST stores
	/// the raw source spelling; escape sequences are resolved here, like in helios.
	inline char charLiteralValue(pst::Access<pst::expr::ExprCharValue> lit) {
		const base::UnescapeResult unescaped
			= base::unescapeString(lit->getValue().value.strView());
		if (!unescaped.has_value()) {
			throw std::runtime_error(
				"Unknown escape sequence in char literal: " + unescaped.error().value
			);
		}
		if (unescaped->value.size() != 1) {
			throw std::runtime_error(
				"Char literal must contain exactly one character, got '" + unescaped->value + "'"
			);
		}
		return unescaped->value.front();
	}
}
