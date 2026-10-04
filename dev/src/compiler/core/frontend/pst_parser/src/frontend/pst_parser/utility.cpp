#include "utility.hpp"

#include "element_kind.hpp"
#include "lang_parser_element.hpp"

#include <frontend/pst_parser/elements/hierarchy/statements/expr_stmt.hpp>
#include <frontend/pst_parser/elements/includes/basic.hpp>

namespace pst {
	base::Optional<AccessLocked<Stmt>> extractSingleStatement(
		query::Context& ctx, const AccessLocked<LangElement>& root
	) {
		auto root_elem = root.unlock(ctx);
		auto children  = root_elem->viewChildren();

		auto it = children.begin();
		if (it == children.end()) return {};

		auto first = it;
		++it;
		if (it != children.end()) return {};

		auto stmt_opt = (*first).unlock(ctx).dynamicCast<Stmt>();
		if (!stmt_opt.has_value()) return {};

		return stmt_opt.value();
	}

	base::Optional<AccessLocked<ExprStmt>> extractSingleExpression(
		query::Context& ctx, const AccessLocked<LangElement>& root
	) {
		auto stmt_opt = extractSingleStatement(ctx, root);
		if (!stmt_opt.has_value()) return {};

		auto stmt = stmt_opt.value().unlock(ctx);
		if (stmt->getElementKind() != ElementKind::ExprStmt) return {};

		return stmt_opt.value().dynamicCast<ExprStmt>();
	}

	base::Optional<AccessLocked<Stmt>> extractSingleInstruction(
		query::Context& ctx, const AccessLocked<LangElement>& root
	) {
		auto stmt_opt = extractSingleStatement(ctx, root);
		if (!stmt_opt.has_value()) return {};

		auto first_child = stmt_opt.value().unlock(ctx);

		auto kind = first_child->getElementKind();
		if (kind != ElementKind::If && kind != ElementKind::While && kind != ElementKind::For
		    && kind != ElementKind::Block)
			return {};

		return stmt_opt.value();
	}

	base::Optional<AccessLocked<Stmt>> extractSingleDefinition(
		query::Context& ctx, const AccessLocked<LangElement>& root
	) {
		auto stmt_opt = extractSingleStatement(ctx, root);
		if (!stmt_opt.has_value()) return {};

		if (stmt_opt.value().unlock(ctx)->isDeclaration() == DeclKind::None) return {};

		return stmt_opt.value();
	}
}
