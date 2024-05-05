#include "rift_parser_base.hpp"
#include <base/exceptions.hpp>
#include <base/str_utils.hpp>

namespace pst {
	void RiftParserState::addImport(const tpc::ParserCBorrowRef<pst::Import>& import) {
		imports.push_back(import);
	}

	const dia::SourcePosition& RiftElement::getSourcePosition() const { return source_position; }

}
