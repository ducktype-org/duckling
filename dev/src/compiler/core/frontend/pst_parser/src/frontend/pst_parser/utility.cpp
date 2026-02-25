#include "utility.hpp"

#include "element_kind.hpp"
#include "elements/hierarchy/statements/expr_stmt.hpp"
#include "elements/includes/basic.hpp"
#include "lang_parser_element.hpp"

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
		if (it == children.end()) return {};

		auto first_child = (*it).unlock(ctx);
		++it;

		if (it == children.end() && first_child->getElementKind() == ElementKind::ExprStmt)
			return (*children.begin()).template dynamicCast<ExprStmt>();

		return {};
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
