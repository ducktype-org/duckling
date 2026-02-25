#pragma once

#include <base/collections/optional.hpp>

#include <diagnostic/source_position.hpp>

namespace query {
	struct Context;
}

namespace pst {
	class LangElement;
	class ExprStmt;
	template<typename>
	class AccessLocked;

	/**
	 * @brief If @p root contains exactly one child and that child is an ExprStmt, returns it.
	 *
	 * Useful for distinguishing a standalone expression input (to be evaluated and printed)
	 * from a definition input (function, variable, class, etc.).
	 *
	 * @param ctx  Query context for PST access
	 * @param root The PST root element to examine
	 * @return The single ExprStmt if present, empty otherwise
	 */
	base::Optional<AccessLocked<ExprStmt>> extractSingleExpression(
		query::Context& ctx, const AccessLocked<LangElement>& root
	);
}

namespace pst::internal {
	void printHighlight(dia::SourcePosition pos, const std::string& message);
}
