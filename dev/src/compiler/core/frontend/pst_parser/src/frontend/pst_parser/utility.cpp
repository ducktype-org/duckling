#include "utility.hpp"

#include "element_kind.hpp"
#include "lang_parser_element.hpp"

#include <frontend/pst_parser/elements/hierarchy/statements/expr_stmt.hpp>
#include <frontend/pst_parser/elements/includes/basic.hpp>

#include <diagnostic/diagnostic_converters.hpp>
#include <diagnostic/message.hpp>
#include <printer/stream_printer.hpp>

namespace pst {
	base::Optional<AccessLocked<ExprStmt>> extractSingleExpression(
		query::Context& ctx, const AccessLocked<LangElement>& root
	) {
		auto root_elem = root.unlock(ctx);
		auto children  = root_elem->viewChildren();

		auto it = children.begin();
		if (it == children.end()) return {};  // root has no children

		auto first_child = (*it).unlock(ctx);
		++it;  // advance to check whether there is a second child

		// exactly one child and it is an expression statement
		if (it == children.end() && first_child->getElementKind() == ElementKind::ExprStmt)
			return (*children.begin()).dynamicCast<ExprStmt>();

		return {};
	}

	base::Optional<AccessLocked<Stmt>> extractSingleInstruction(
		query::Context& ctx, const AccessLocked<LangElement>& root
	) {
		auto root_elem = root.unlock(ctx);
		auto children  = root_elem->viewChildren();

		// same iterator logic as above.
		auto it = children.begin();
		if (it == children.end()) return {};

		auto first_child = (*it).unlock(ctx);
		++it;
		if (it != children.end()) return {};

		auto kind = first_child->getElementKind();
		if (kind != ElementKind::If && kind != ElementKind::While && kind != ElementKind::For
		    && kind != ElementKind::Block)
			return {};

		return (*children.begin()).dynamicCast<Stmt>();
	}

	base::Optional<AccessLocked<Stmt>> extractSingleTopLevelStatement(
		query::Context& ctx, const AccessLocked<LangElement>& root
	) {
		auto root_elem = root.unlock(ctx);
		auto children  = root_elem->viewChildren();

		auto it = children.begin();
		if (it == children.end()) return {};

		auto first_child = (*it).unlock(ctx);
		++it;
		if (it != children.end()) return {};

		auto kind = first_child->getElementKind();

		// Reject expression statements (already handled by extractSingleExpression)
		if (kind == ElementKind::ExprStmt) return {};

		// Reject control flow instructions (already handled by extractSingleInstruction)
		if (kind == ElementKind::If || kind == ElementKind::While || kind == ElementKind::For
		    || kind == ElementKind::Block)
			return {};

		return (*children.begin()).dynamicCast<Stmt>();
	}
}

namespace pst::internal {
	void printHighlight(dia::SourcePosition pos, const std::string& message) {
		auto note = makeBox<dia::PlaceholderMessage<dia::Hint, dia::Message::Domain::Parser>>(
			pos, message
		);
		auto content = dia::DiagnosticToUserConverter::toPrinterContents(note.ref(), true);
		printer::StreamPrinter::printNL(content, std::cerr);
	}
}
