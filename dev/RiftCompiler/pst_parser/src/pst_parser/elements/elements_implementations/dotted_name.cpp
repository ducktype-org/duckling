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
		auto position = state.ctokens().peek().getPosition();
		auto out      = makeRef<DottedName>(position);
		do {
			if (state.ctokens().peek().isIdentifier()) {
				tpc::Identifier next;
				tpc::parseOne(state, &next);
				out->names.push_back(next);
			} else {
				state.fail(-1, "expected identifier after here");
				break;
			}
		} while (state.tryEat(rift_def::Operator::Period));

		if (state.tryEat(rift_def::Operator::PeriodStar)) out->star = true;

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
