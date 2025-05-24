#include "../../hierarchy/class_elements/class_special.hpp"

#include "../../hierarchy/class_elements/constructor.hpp"
#include "../../hierarchy/class_elements/copy_constructor.hpp"
#include "../../hierarchy/class_elements/destructor.hpp"
#include "../../hierarchy/lists/initializer_list.hpp"     // IWYU pragma: keep
#include "../../hierarchy/lists/parameter_list.hpp"       // IWYU pragma: keep
#include "../../hierarchy/not_statements/code_block.hpp"  // IWYU pragma: keep
#include "../../hierarchy/not_statements/fun_param.hpp"   // IWYU pragma: keep
#include "preamble.hpp"

namespace pst {
	MBox<ClassSpecial> ClassSpecial::parse(LangParserState& state, const ClassContext& ctx) {
		i64 skip = ClassStmt::countSpecifiers(state);


		if (state[skip + 1].isBracketGroup(Token::Round)) return Constructor::parse(state, ctx);

		if (state[skip + 2].isStr(base::StrID{ "destroy" })) return Destructor::parse(state, ctx);
		if (state[skip + 2].is(Keyword::Copy)) return CopyConstructor::parse(state, ctx);

		return Constructor::parse(state, ctx);
	}
}
