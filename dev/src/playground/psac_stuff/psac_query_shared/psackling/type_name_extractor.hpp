#pragma once

#include "context.hpp"

#include <frontend/pst_parser/elements/hierarchy/lists/all_lists.hpp>  // Stupid PST imports
#include <frontend/pst_parser/elements/hierarchy/not_statements/all_not_statements.hpp>

#include <string_id/string_id.hpp>

namespace pg {
	base::StrID extractTypeName(const pst::ExprElement& expr);

	/// Result of resolving a type expression.
	struct ResolvedType {
		tp::TypeID type_id{};
		/// Set when the outermost array dimension is a runtime expression (C99-style
		/// VLA): type_id is then a VLA array type and the caller must lower this
		/// count expression at the declaration site.
		const pst::ExprElement* vla_count = nullptr;
	};

	/// Resolves a type expression to a TypeID: a bare identifier/keyword (`i64`,
	/// `Point`) or a C-style array `T[N]` / `T[N][M]`, where sizes are positive
	/// compile-time constants. With `allow_vla`, the outermost size may instead be a
	/// runtime expression (inner sizes must stay constant, as in C).
	ResolvedType resolveTypeExpr(
		Context& ctx, const pst::ExprElement& expr, const std::string& what, bool allow_vla = false
	);
}
