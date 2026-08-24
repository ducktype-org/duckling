
#include "../../../hierarchy/not_statements/format_string_sub_elements/format_sub_string.hpp"

#include "../preamble.hpp"

namespace pst {
	MBox<FormatSubString> FormatSubString::parse(LangParserState& state) {
		auto out = makeBox<FormatSubString>(state);

		CORE_ASSERT(
			state[0].is(Token::Type::FormatStringSubString), "Bad format sub element choice"
		);

		out->string = state[0].getValue();
		PARSE().eatOne();

		PST_RETURN out;
	}

	void FormatSubString::dprint(std::ostream& out) const {
		out << "{";
		std::print(out, R"("string": "{}")", string.value.strView());
		out << "}";
	}

	HashAlg& FormatSubString::addElementDataToStableHash(HashAlg& partial_hash) const {
		hashing::addToHash(partial_hash, string);
		return partial_hash;
	}
}
