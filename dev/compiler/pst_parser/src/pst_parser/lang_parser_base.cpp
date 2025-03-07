#include "access.hpp"
#include "lang_parser_element.hpp"
#include "lang_parser_state.hpp"
#include <base/exceptions.hpp>
#include <base/str_utils.hpp>

namespace pst {
	AccessLocked<LangElement> LangElement::getParent() const { return { parent->toMRef() }; }

	void LangParserState::addImport(const ImportType& import) { imports.push_back(import); }

	const dia::SourcePosition& LangElement::getSourcePosition() const { return source_position; }

	void LangElement::addToken(base::c_borrow_ptr<tpc::Token> t) {
		CORE_ASSERT(t != nullptr, "All tokens that are part of an element should exist.");
		sub_elements.emplace_back(t);
		setLastToken(t->getPosition());
	}

	void LangElement::addToken(const base::unique_ptr<tpc::Token>& t) { addToken(t.borrow()); }

	void LangElement::addToken(const tpc::Token& t) { addToken(base::borrow_ptr(&t)); }

	void LangElement::addChild(MRef<LangElement> el) {
		auto opt = el.toOpt();
		if (opt) {
			sub_elements.emplace_back(opt.value());
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
}
