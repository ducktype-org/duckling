/**
 * @file coercion_error.hpp
 * @brief Why a coercion may not be performed abd in which place it went wrong. A failed coercion is
 * a tree- like structure for more complex coercions, which allows for better error messages.
 *
 * @TODO: #3691 Generalize it into a type error tree to use it to provide constructability and
 * copyability errors.
 */

#pragma once

#include "../symbol_type.hpp"
#include "coercion_path.hpp"

#include <iosfwd>
#include <string_view>
#include <variant>
#include <vector>

namespace compiler::tsh::coercions {
	namespace coercion_error {
		/**
		 * @brief The types just don't match
		 */
		struct IncompatibleTypes final {};

		/**
		 * @brief The target needs a mutable value but we pass an immutable.
		 * @TODO: #1488 Expand this comment with the actual semantics.
		 */
		struct MutabilityMismatch final {};

		/**
		 * @brief The types on this level of the tree fit, but something in their sub-tree does not.
		 * This reason basically means "something bad happened lower in the tree, look down to
		 * `causes` for the exact error".
		 * @see `CoercionError` doc comment for more info.
		 */
		struct SubPartRefused final {};

		/**
		 * @brief The types are meant to be coercible (in the future), but no rule describes how to
		 * coerce them yet. Purely for better error purposes.
		 */
		struct NotImplemented final {};

		/**
		 * @brief The value can be coerced into many alternatives equally well.
		 * @note For now just used by variants, in the future may be used to declare ambiguous user
		 * conversions.
		 * @TODO: #3656 Make this error be returned by ambiguous user conversions.
		 */
		struct AmbiguousCoercion final {
			/// The indicies of variant alternatives that the type may be wrapped as.
			std::vector<usize> candidates;
		};

		/**
		 * @brief The coercion needed a copy of a value whose type is not copyable.
		 */
		struct TypeNotCopyable final {};

		/**
		 * @brief The coercion required a copy of a non-trivially copyable type, it has to be done
		 * explicitly by `copy` or `move`.
		 */
		struct RequiresExplicitCopyMove final {};
	}

	using CoercionFailure = std::variant<
		coercion_error::IncompatibleTypes,
		coercion_error::MutabilityMismatch,
		coercion_error::SubPartRefused,
		coercion_error::NotImplemented,
		coercion_error::AmbiguousCoercion,
		coercion_error::TypeNotCopyable,
		coercion_error::RequiresExplicitCopyMove>;

	/**
	 * @brief A tree-like structure explaining why a coercion was refused, going all the way down
	 * to the exact place that caused it.
	 *
	 * For example for a `(i32, (bool, string)) -> (i64, (bool, i32))` coercion error tree will look
	 * like this:
	 * ```
	 * CoercionError {
	 *     reason = SubPartRefused;
	 *     source = (i32, (bool, string));
	 *     target = (i64, (bool, i32));
	 *     causes = [
	 *         Cause {
	 *             at = { Component, 1 };
	 *             error = CoercionError {
	 *                 reason = SubPartRefused;
	 *                 source = (bool, string);
	 *                 target = (bool, i32);
	 *                 causes = [
	 *                     Cause {
	 *                         at = { Component, 1 };
	 *                         error = CoercionError {
	 *                             reason = IncompatibleTypes;
	 *                             source = string;
	 *                             target = i32;
	 *                             causes = [];
	 *                         }
	 *                     }
	 *                 ]
	 *             }
	 *         }
	 *     ]
	 * }
	 * ```
	 */
	struct CoercionError final {
		/**
		 * @brief A refusal this one follows from, and where in the coercion above it it sits.
		 */
		struct Cause;

		/// What went wrong at this tree level.
		CoercionFailure reason;

		/// The coercion at this tree level.
		SymbolType<> source;
		SymbolType<> target;

		/// The refusals of the sub-coercions. It has no default member initializer, because
		/// `Cause` is not complete yet, so every refusal names its causes, even when it has none.
		std::vector<Cause> causes;

		/**
		 * @brief The whole coercion error.
		 *
		 * The first line is written directly, every line after it is prefixed with @p indent.
		 */
		void debugPrint(std::ostream& out, std::string_view indent = "") const;
	};

	struct CoercionError::Cause final {
		/// Where in the coercion above it this refusal sits.
		CoercionPath at;

		/// The refusal itself.
		CoercionError error;
	};
}
