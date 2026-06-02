#pragma once

#include "../statements/action.hpp"

namespace pst {
	/**
	 * @todo Should throw be an action?
	 */
	class Throw final: public Action {
		PARENT_CLASS(Action);
		CLONE_SIGNATURE(Throw) override;
	public:
		ELEMENT_CLONE_DECL(Throw);

		explicit Throw(const LangParserState& state): Action(state) {}

		void dprint(std::ostream& out) const final;
		~Throw() final = default;

		void acceptVisitor(PstVisitor& visitor) const override;
	};
}
