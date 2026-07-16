#pragma once

#include "../statements/action.hpp"

namespace pst {
	class Break final: public Action {
		SIMPLE_FINAL_ELEMENT_CLASS_PREAMBLE(Break, Action);

	public:
		explicit Break(LangElementConstructionArgument state): Action(state) {}

		void dprint(std::ostream& out) const final;
		~Break() final = default;

		void acceptVisitor(PstVisitor& visitor) const override;
	};
}
