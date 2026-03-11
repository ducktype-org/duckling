#include "../../hierarchy/not_statements/dotted_name.hpp"

#include "preamble.hpp"

namespace pst {

	bool DottedName::getStar() const { return star; }

	MBox<DottedName> DottedName::parse(LangParserState& state) {
		auto out = makeBox<DottedName>(state);
		do {
			bool            is_id = state[0].isIdentifier();
			tpc::Identifier next;
			PARSE().one(&next);
			if (is_id) out->names.push_back(next);
			// If not special meaning, assume wrong type
			else if (!state[0].is(lang_def::NamedOperator::Period)
			         && !state[0].is(lang_def::NamedOperator::PeriodStar)
			         && !state[0].is(lang_def::Special::Semicolon)) {
				state.tokens().next();
			}
		}
		PST_WHILE(PARSE().tryEat(lang_def::NamedOperator::Period));

		if (PARSE().tryEat(lang_def::NamedOperator::PeriodStar)) out->star = true;

		PST_RETURN out;
	}

	void DottedName::dprint(std::ostream& out) const {
		out << "{";

		if (star)
			out << R"("star": "true",)";
		else
			out << R"("star": "false",)";

		out << R"("names": [)";

		for (const auto& name: names) {
			tpc::nullAwareDprint(name, out);
			out << ", ";
		}

		out << "]}";
	}

	HashAlg& DottedName::addElementDataToStableHash(HashAlg& partial_hash) const {
		addToHash(partial_hash, names);
		addToHash(partial_hash, star);
		return partial_hash;
	}
}
