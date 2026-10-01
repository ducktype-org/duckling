/**
 * @file passing.hpp
 * @brief How a value may be handed over to a new owner.
 */

#pragma once

#include "../expression_type.hpp"

#include <cstdint>

namespace compiler::tsh::coercions {
	/**
	 * @brief How a value may be handed over.
	 */
	enum class PassingMethod : uint8_t {
		/// Trivially copyable, copied by copying its bytes.
		ByteCopy,
		/// An owned rvalue, moved implicitly.
		ImplicitMove,
		/// A non-trivially copyable lvalue. The ownership transfer has to be explicit by `copy`/`move`.
		ExplicitCopyOrMove,
		/// A non-trivially copyable lvalue whose type has no copy constructor.
		NotCopyable,
	};

	/**
	 * @brief Decides how the given value may be handed over, based on where it came from.
	 */
	[[nodiscard]] PassingMethod passingMethod(query::Context& ctx, const ExpressionType<>& value);

	/**
	 * @brief Whether every value of @p type is handed over as `ByteCopy`, whatever its value
	 * category.
	 */
	[[nodiscard]] bool isCopiedByBytes(query::Context& ctx, const SymbolType<>& type);
}
