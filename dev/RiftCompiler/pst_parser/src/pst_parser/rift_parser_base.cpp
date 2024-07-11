#include "rift_parser_base.hpp"
#include <base/exceptions.hpp>
#include <base/str_utils.hpp>

namespace pst {
	void RiftParserState::addImport(const tpc::ParserCBorrowRef<pst::Import>& import) {
		imports.push_back(import);
	}

	const dia::SourcePosition& RiftElement::getSourcePosition() const { return source_position; }

	const std::vector<dia::SourcePosition>& RiftElement::getKeywordPositions() const {
		return keyword_positions;
	}

	void RiftElement::addKeyword(dia::SourcePosition pos) { keyword_positions.push_back(pos); }

	void RiftElement::setLastToken(dia::SourcePosition pos) {
		if (pos.getEnd() > source_position.getEnd())
			source_position = dia::SourcePosition(source_position, pos.getEnd());
	}

}
