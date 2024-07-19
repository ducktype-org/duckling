#include "rift_parser_element.hpp"
#include "rift_parser_state.hpp"
#include <base/exceptions.hpp>
#include <base/str_utils.hpp>

namespace pst {
	void RiftParserState::addImport(const tpc::ParserCBorrowRef<pst::Import>& import) {
		imports.push_back(import);
	}

	const dia::SourcePosition& RiftElement::getSourcePosition() const { return source_position; }

	void RiftElement::addToken(base::c_borrow_ptr<tpc::Token> t) {
		RIFT_ASSERT(t != nullptr, "All tokens that are part of an element should exist.");
		sub_elements.emplace_back(t);
		setLastToken(t->getPosition());
	}

	void RiftElement::addToken(const base::unique_ptr<tpc::Token>& t) { addToken(t.borrow()); }

	void RiftElement::addToken(const tpc::Token& t) { addToken(base::borrow_ptr(&t)); }

	void RiftElement::addChild(ParserCBorrowRef<RiftElement> el) {
		if (el != nullptr) {
			sub_elements.emplace_back(el);
			setLastToken(el->getSourcePosition());
		}
	}

	void RiftElement::setLastToken(dia::SourcePosition pos) {
		if (pos.getEnd() > source_position.getEnd())
			source_position = dia::SourcePosition(source_position, pos.getEnd());
	}

}
