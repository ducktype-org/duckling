#pragma once

#include "../statements/action.hpp"

namespace pst {
	class Defer final: public Action {
		SIMPLE_FINAL_ELEMENT_CLASS_PREAMBLE(Defer, Action);

	public:
		explicit Defer(LangElementConstructionArgument state): Action(state) {}

		void dprint(std::ostream& out) const final;
		~Defer() final = default;

		void acceptVisitor(PstVisitor& visitor) const override;
	};
}
