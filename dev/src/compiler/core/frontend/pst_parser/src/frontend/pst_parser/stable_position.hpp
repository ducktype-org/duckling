#pragma once

#include <base/types/bit256.hpp>

#include <diagnostic/source_position.hpp>
#include <query_framework/context/context.hpp>

namespace pst {
	class LangElement;

	class SourcePositionLocked;

	/**
	 * @brief Class that represents a position in the source code that
	 * that is stable across re-parses, if the order of the elements
	 * that are inside the StablePosition does not change.
	 *
	 * This is different from the normal SourcePosition where
	 * the position can become invalid after re-parses.
	 *
	 * @warning If used to contain a range from one element to another,
	 * the position will become invalid if the order of the elements changes.
	 * So it is safe to have elements like `<AccessExpr>, <CallExpr> in .a(x)`,
	 * but not safe to have like range from one `<FunDecl>` to another in the same file `<FunDecl>`.
	 */
	class StablePosition final {
	public:
		using HashType = base::Bit256;

	private:
		StablePosition(HashType begin_scope_node, base::Optional<HashType> end_scope_node):
			  begin_scope_node(begin_scope_node),
			  end_scope_node(end_scope_node) {}

		friend LangElement;

	public:
		/**
		 * @brief Node hash that defines start of the position range.
		 */
		HashType begin_scope_node;

		/**
		 * @brief Node hash that defines the end of the position range.
		 * If not set, the position is defined as the position of the @p begin_scope_node only.
		 */
		base::Optional<HashType> end_scope_node;

		/**
		 * @brief Get the source position from
		 * the most recently parsed nodes.
		 */
		[[nodiscard]] dia::SourcePosition getActiveSourcePosition(query::Context& ctx) const;

		/**
		 * @brief Get the source position access locked
		 * the most recently parsed nodes.
		 */
		[[nodiscard]] SourcePositionLocked getActiveSourcePositionLocked(query::Context& ctx) const;

		/**
		 * @brief Same as  `getActiveSourcePosition` but uses illegalAccess, so only to be used
		 * outside query.
		 */
		[[nodiscard]] dia::SourcePosition getActiveSourcePositionIllegalAccess() const;

		/**
		 * @brief Inplace extend the position to include the position of another StablePosition.
		 * @warning This method assumes that the order of the nodes will never change after
		 * recompilation.
		 */
		void extendWithSubsequentPos(const StablePosition& other);

		/**
		 * @brief Create a new StablePosition that is the extension of this position and another
		 * position.
		 * @warning This method assumes that the order of the nodes will never change after
		 * recompilation.
		 */
		[[nodiscard]] StablePosition extendedWithSubsequentPos(const StablePosition& other) const;
	};
}
