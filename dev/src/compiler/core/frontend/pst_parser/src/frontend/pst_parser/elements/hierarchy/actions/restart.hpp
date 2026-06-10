#pragma once

#include "../statements/action.hpp"

namespace pst {
	class Restart final: public Action {
		PARENT_CLASS(Action);
		THIS_CLASS(Restart);
		CLONE_SIGNATURE_DEFAULT_OVERRIDE();
	public:
		ELEMENT_CLONE_DECL(Restart);

	public:
		explicit Restart(const LangParserState& state): Action(state) {}

		void dprint(std::ostream& out) const final;
		~Restart() final = default;

		void acceptVisitor(PstVisitor& visitor) const override;
	};
}
