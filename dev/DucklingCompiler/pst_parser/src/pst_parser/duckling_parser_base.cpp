#include "duckling_parser_element.hpp"
#include "duckling_parser_state.hpp"
#include <base/exceptions.hpp>
#include <base/str_utils.hpp>

namespace pst {
	void DucklingParserState::addImport(const tpc::ParserCBorrowRef<pst::Import>& import) {
		imports.push_back(import);
	}

	const dia::SourcePosition& DucklingElement::getSourcePosition() const { return source_position; }

	void DucklingElement::addToken(base::c_borrow_ptr<tpc::Token> t) {
		DUCKLING_ASSERT(t != nullptr, "All tokens that are part of an element should exist.");
		sub_elements.emplace_back(t);
		setLastToken(t->getPosition());
	}

	void DucklingElement::addToken(const base::unique_ptr<tpc::Token>& t) { addToken(t.borrow()); }

	void DucklingElement::addToken(const tpc::Token& t) { addToken(base::borrow_ptr(&t)); }

	void DucklingElement::addChild(ParserBorrowRef<DucklingElement> el) {
		if (el != nullptr) {
			sub_elements.emplace_back(el);
			setLastToken(el->getSourcePosition());
		}
	}

	void DucklingElement::setLastToken(dia::SourcePosition pos) {
		if (pos.getEnd() > source_position.getEnd())
			source_position = dia::SourcePosition(source_position, pos.getEnd());
	}

}
