#pragma once

#include "../statements/action.hpp"

namespace pst {
	class Defer final: public Action {
		PARENT_CLASS(Action);
		CLONE_SIGNATURE(Defer) override;
	public:
		ELEMENT_CLONE_DECL(Defer);

	public:
		explicit Defer(const LangParserState& state): Action(state) {}

		void dprint(std::ostream& out) const final;
		~Defer() final = default;

		void acceptVisitor(PstVisitor& visitor) const override;
	};
}
