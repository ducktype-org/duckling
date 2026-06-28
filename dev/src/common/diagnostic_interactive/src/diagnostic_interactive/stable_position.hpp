#pragma once
#include <base/collections/optional.hpp>
#include <base/types/bit256.hpp>

#include <diagnostic/source_position.hpp>

namespace query {
	struct Context;
}

namespace dia_int {
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
		/**
		 * @brief Node hash that defines start of the position range.
		 */
		HashType begin_node;

		/**
		 * @brief Node hash that defines the end of the position range.
		 * If not set, the position is defined as the position of the @p begin_scope_node only.
		 */
		base::Optional<HashType> end_node;

		using ToSourcePosIllegalAccessFunc = dia::SourcePosition (*)(const StablePosition&);
		/**
		 * @brief Pointer to the function that converts this StablePosition to a
		 * SourcePosition without access to the query context.
		 *
		 * This is a pointer because I don't want to add the depedency on the PST from the
		 * DiagnosticInteractive module, but the module has to be able to call the conversion
		 * function.
		 */
		ToSourcePosIllegalAccessFunc to_source_pos_illegal_access_fn;

		using ToSourcePosFuncWithContextFunc
			= dia::SourcePosition (*)(query::Context&, const StablePosition&);

		/**
		 * @brief Pointer to the function that converts this StablePosition to a
		 * SourcePosition with access to the query context.
		 */
		ToSourcePosFuncWithContextFunc to_source_pos_with_context_fn;

		StablePosition(
			ToSourcePosFuncWithContextFunc to_source_pos_with_context_fn,
			ToSourcePosIllegalAccessFunc   to_source_pos_illegal_access_fn,
			HashType                       begin_node,
			base::Optional<HashType>       end_node = {}
		):
			  begin_node(begin_node),
			  end_node(end_node),
			  to_source_pos_illegal_access_fn(to_source_pos_illegal_access_fn),
			  to_source_pos_with_context_fn(to_source_pos_with_context_fn) {}

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

		/**
		 * @brief Get the active source position corresponding to this stable position,
		 * bypasses the query context.
		 */
		[[nodiscard]] dia::SourcePosition getActiveSourcePositionIllegalAccess() const {
			return to_source_pos_illegal_access_fn(*this);
		}

		/**
		 * @brief Get the active source position corresponding to this stable position, with access
		 * to the query context.
		 */
		[[nodiscard]] dia::SourcePosition getActiveSourcePosition(query::Context& ctx) const {
			return to_source_pos_with_context_fn(ctx, *this);
		}

		static StablePosition fakePosition();

		/**
		 * @brief Two positions are equal when they span the same source nodes.
		 * The conversion-function pointers are not part of the identity.
		 */
		bool operator==(const StablePosition& other) const {
			return begin_node == other.begin_node && end_node == other.end_node;
		}
	};
}
