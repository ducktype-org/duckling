#include "../../hierarchy/class_elements/class_special.hpp"

#include "../../hierarchy/class_elements/constructor.hpp"
#include "../../hierarchy/class_elements/copy_constructor.hpp"
#include "../../hierarchy/class_elements/destructor.hpp"
#include "../../hierarchy/not_statements/code_block.hpp"  // IWYU pragma: keep
#include "preamble.hpp"

namespace pst {
	MBox<ClassSpecial> ClassSpecial::parse(LangParserState& state) {
		if (state[1].isBracketGroup(Token::Round)) return Constructor::parse(state);

		if (state[2].isStr(base::StrID{ "destroy" })) return Destructor::parse(state);
		if (state[2].is(Keyword::Copy)) return CopyConstructor::parse(state);

		return Constructor::parse(state);
	}

	HashAlg& ClassSpecial::addElementDataToStableHash(HashAlg& partial_hash) const {
		return partial_hash;
	}
}
