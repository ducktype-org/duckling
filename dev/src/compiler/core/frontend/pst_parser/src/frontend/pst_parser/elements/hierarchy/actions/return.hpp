#pragma once

#include "../statements/action.hpp"

namespace pst {
	class Return final: public Action {
		PARENT_CLASS(Action);
		CLONE_SIGNATURE(Return) override;
	public:
		ELEMENT_CLONE_DECL(Return);

	public:
		explicit Return(const LangParserState& state): Action(state) {}

		void dprint(std::ostream& out) const final;
		~Return() final = default;

		void acceptVisitor(PstVisitor& visitor) const override;
	};
}
