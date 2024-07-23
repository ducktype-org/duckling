#include "elements_implementation.hpp"
#include "pst_parser/elements/elements.hpp"

namespace pst {
	std::vector<base::StrId> DottedName::getNames() const {
		std::vector<base::StrId> out;
		out.reserve(names.size());
		for (auto name: names) out.push_back(base::StrId(name));
		return out;
	}

	bool DottedName::getStar() const { return star; }

	ParserRef<DottedName> DottedName::parse(RiftParserState& state) {
		auto position = state.getPosition();
		auto out      = makeRef<DottedName>(position);
		do {
			bool            is_id = state[0].isIdentifier();
			tpc::Identifier next;
			state.parse(out).one(&next, true);
			if (is_id) out->names.push_back(next);
			// If not special meaning, assume wrong type
			else if(!state[0].is(rift_def::Operator::Period) && 
					!state[0].is(rift_def::Operator::PeriodStar) && 
					!state[0].is(rift_def::Special::Semicolon)) {
				state.tokens().next();
			}
		} while (state.parse(out).tryEat(rift_def::Operator::Period));

		if (state.parse(out).tryEat(rift_def::Operator::PeriodStar)) out->star = true;

		return out;
	}

	void DottedName::dprint(std::ostream& out) const {
		out << "{\"DottedName\": {";

		if (star)
			out << R"("star": "true",)";
		else
			out << R"("star": "false",)";

		out << R"("names": [)";

		for (const auto& name: names) {
			tpc::nullAwareDprint(name, out);
			out << ", ";
		}

		out << "]}}";
	}
}
