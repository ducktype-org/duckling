#pragma once

#include "../statements/action.hpp"

namespace pst {
	class Continue final: public Action {
		PARENT_CLASS(Action);
		CLONE_SIGNATURE(Continue) override;
	public:
		ELEMENT_CLONE_DECL(Continue);

		explicit Continue(const LangParserState& state): Action(state) {}

		void dprint(std::ostream& out) const final;
		~Continue() final = default;

		void acceptVisitor(PstVisitor& visitor) const override;
	};
}
