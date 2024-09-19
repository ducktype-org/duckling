#include "preamble.hpp"

namespace pst {
	tpc::ParserRef<ClassSpecial>
		ClassSpecial::parse(RiftParserState& state, const ClassContext& ctx) {
		i64 skip = ClassStmt::countSpecifiers(state);


		if (state[skip + 1].isBracketGroup(Token::Round)) return Constructor::parse(state, ctx);

		if (state[skip + 2].isStr(base::StrID{ "destroy" })) return Destructor::parse(state, ctx);

		return Constructor::parse(state, ctx);
	}
}
