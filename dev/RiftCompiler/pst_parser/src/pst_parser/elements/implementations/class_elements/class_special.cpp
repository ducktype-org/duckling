#include "preamble.hpp"

namespace pst {
	tpc::ParserRef<ClassSpecial> ClassSpecial::parse(RiftParserState& state) {
		i64 skip = ClassStmt::countSpecifiers(state);


		if (state[skip + 1].isBracketGroup(Token::Round)) {
			return Constructor::parse(state);
		}		

		if (state[skip + 2].isStr(base::StrId{"destroy"})) {
			return Destructor::parse(state);
		}		

		return Constructor::parse(state);
	}
}