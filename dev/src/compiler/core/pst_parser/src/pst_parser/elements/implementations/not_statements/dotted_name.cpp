#include "../../hierarchy/not_statements/dotted_name.hpp"

#include "preamble.hpp"

namespace base::extend {
	void BoxPtrDeleter<pst::DottedName>::del(pst::DottedName* ptr) {
		delete ptr;
	}
}

namespace pst {

	bool DottedName::getStar() const { return star; }

	MBox<DottedName> DottedName::parse(LangParserState& state) {
		auto position = state.getPosition();
		auto out      = makeBox<DottedName>(position);
		do {
			bool            is_id = state[0].isIdentifier();
			tpc::Identifier next;
			state.parse(out).one(&next, true);
			if (is_id) out->names.push_back(next);
			// If not special meaning, assume wrong type
			else if (!state[0].is(lang_def::NamedOperator::Period)
			         && !state[0].is(lang_def::NamedOperator::PeriodStar)
			         && !state[0].is(lang_def::Special::Semicolon)) {
				state.tokens().next();
			}
		} while (state.parse(out).tryEat(lang_def::NamedOperator::Period));

		if (state.parse(out).tryEat(lang_def::NamedOperator::PeriodStar)) out->star = true;

		return out;
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
}
