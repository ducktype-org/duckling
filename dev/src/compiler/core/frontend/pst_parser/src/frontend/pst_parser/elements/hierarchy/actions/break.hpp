#pragma once

#include "../statements/action.hpp"

namespace pst {
	class Break final: public Action {
		PARENT_CLASS(Action);
		CLONE_SIGNATURE(Break) override;
	public:
		ELEMENT_CLONE_DECL(Break);

		explicit Break(const LangParserState& state): Action(state) {}

		void dprint(std::ostream& out) const final;
		~Break() final = default;

		void acceptVisitor(PstVisitor& visitor) const override;
	};
}
