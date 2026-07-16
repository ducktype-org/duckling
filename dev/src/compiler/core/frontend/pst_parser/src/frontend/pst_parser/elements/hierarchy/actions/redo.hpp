#pragma once

#include "../statements/action.hpp"

namespace pst {
	class Redo final: public Action {
		SIMPLE_FINAL_ELEMENT_CLASS_PREAMBLE(Redo, Action);

	public:
		explicit Redo(LangElementConstructionArgument state): Action(state) {}

		void dprint(std::ostream& out) const final;
		~Redo() final = default;

		void acceptVisitor(PstVisitor& visitor) const override;
	};
}
