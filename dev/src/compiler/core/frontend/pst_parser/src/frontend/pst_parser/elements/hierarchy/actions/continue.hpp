#pragma once

#include "../statements/action.hpp"

namespace pst {
	class Continue final: public Action {
		SIMPLE_FINAL_ELEMENT_CLASS_PREAMBLE(Continue, Action);

	public:
		explicit Continue(const LangParserState& state): Action(state) {}

		void dprint(std::ostream& out) const final;
		~Continue() final = default;

		void acceptVisitor(PstVisitor& visitor) const override;
	};
}
