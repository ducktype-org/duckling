#include "access.hpp"
#include "lang_parser_element.hpp"
#include "lang_parser_state.hpp"

#include <base/exceptions.hpp>
#include <base/str_utils.hpp>

namespace pst {
	base::Optional<AccessLocked<LangElement>> LangElement::getParent() const { return parent; }

	void LangParserState::addImport(const ImportType& import) { imports.push_back(import); }

	const dia::SourcePosition& LangElement::getSourcePosition() const { return source_position; }

	void LangElement::addToken(CRef<tpc::Token> t) {
		sub_elements.emplace_back(t);
		setLastToken(t->getPosition());
	}

	void LangElement::addToken(const Box<tpc::Token>& t) { addToken(t.ref()); }

	void LangElement::addToken(const tpc::Token& t) { addToken(&t); }

	void LangElement::addChild(MCRef<LangElement> el) {
		auto opt = el.toOpt();
		if (opt) {
			sub_elements.emplace_back(AccessLocked{ el });
			setLastToken(opt.value()->getSourcePosition());
		}
	}

	void LangElement::setFirstToken(dia::SourcePosition pos) {
		if (pos.getStart() < source_position.getStart())
			source_position = dia::SourcePosition(pos, source_position.getEnd());
	}

	void LangElement::setLastToken(dia::SourcePosition pos) {
		if (not pos.isFileEnd() && pos.getEnd() > source_position.getEnd())
			source_position = dia::SourcePosition(source_position, pos.getEnd());
	}

	void LangElement::acceptVisitor(PstVisitor&) const {
		CORE_PANIC("PstVisitor not supported for " + elementType());
	}

	AccessLocked<LangElement> LangElement::getByID(u64 id) { return pst_id_map.at(id); }
}
