#pragma once

#include "../statements/action.hpp"

namespace pst {
	class Redo final: public Action {
		PARENT_CLASS(Action);
		CLONE_SIGNATURE(Redo) override;
	public:
		ELEMENT_CLONE_DECL(Redo);

	public:
		explicit Redo(const LangParserState& state): Action(state) {}

		void dprint(std::ostream& out) const final;
		~Redo() final = default;

		void acceptVisitor(PstVisitor& visitor) const override;
	};
}
