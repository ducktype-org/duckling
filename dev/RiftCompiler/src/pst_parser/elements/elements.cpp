#include "elements.hpp"

#include <token_parser_core/automatic.hpp>

namespace pst {

	std::vector<base::StrId> DottedName::getNames() const {
		std::vector<base::StrId> out;
		out.reserve(names.size());
		for (auto name : names) {
			out.push_back(base::StrId(name));
		}
		return out;
	}

	void parseDottedName(tpc::ParserState& state, DottedName* d_name) {
		do {
			if (state.ctokens().peek().isIdentifier()) {
				tpc::Identifier next;
				tpc::parseOne(state, &next);
				d_name->names.push_back(next);
			} else {
				state.fail(-1, "expected identifier after here");
				break;
			}
		} while (state.tryEat(rift_def::Operator::Period));

		if (state.tryEat(rift_def::Operator::PeriodStar)) {
			d_name->star = true;
		}
	}
}
