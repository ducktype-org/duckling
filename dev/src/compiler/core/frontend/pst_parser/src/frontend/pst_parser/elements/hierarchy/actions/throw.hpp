#pragma once

#include "../statements/action.hpp"

namespace pst {
	/**
	 * @todo Should throw be an action?
	 */
	class Throw final: public Action {
		SIMPLE_FINAL_ELEMENT_CLASS_PREAMBLE(Throw, Action);
	public:
		explicit Throw(const LangParserState& state): Action(state) {}

		void dprint(std::ostream& out) const final;
		~Throw() final = default;

		void acceptVisitor(PstVisitor& visitor) const override;
	};
}
