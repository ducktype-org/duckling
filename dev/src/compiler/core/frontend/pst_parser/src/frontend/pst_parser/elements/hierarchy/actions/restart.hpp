#pragma once

#include "../statements/action.hpp"

namespace pst {
	class Restart final: public Action {
		PARENT_CLASS(Action);
		CLONE_SIGNATURE(Restart) override;
	public:
		ELEMENT_CLONE_DECL(Restart);

	public:
		explicit Restart(const LangParserState& state): Action(state) {}

		void dprint(std::ostream& out) const final;
		~Restart() final = default;

		void acceptVisitor(PstVisitor& visitor) const override;
	};
}
