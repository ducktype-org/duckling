#include "preamble.hpp"

#include "../../hierarchy/not_statements.hpp"

namespace pst::expr {
	ParserRef<ExprElement> BlockExpr::parse(RiftParserState& state, u64 length) {
		std::cerr << "Parsing Round Group Expression";
		if (!checkLength(state, length)) return nullptr;

		auto out = base::make_unique<BlockExpr>(state.getPosition());

		state.parse(out).one(&out->block);

		return out;
	}
}
