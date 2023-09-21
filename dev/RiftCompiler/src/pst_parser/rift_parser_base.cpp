#include "rift_parser_base.hpp"
#include <base/exceptions.hpp>
#include <base/str_concat.hpp>

namespace pst {
	void RiftParserState::addImport(tpc::ParserCBorrowRef<pst::Import> import) {
		imports.push_back(import);
	}

	const lexer::SourcePosition& RiftElement::getSourcePosition() const {
		return source_position;
	}

	const RiftParserState::ImportType& RiftParserState::getImports() const {
		return imports;
	};

}
