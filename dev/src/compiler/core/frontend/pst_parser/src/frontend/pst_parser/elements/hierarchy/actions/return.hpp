#pragma once

#include "../statements/action.hpp"

namespace pst {
	class Return final: public Action {
	public:
		explicit Return(const LangParserState& state): Action(state) {}

		void dprint(std::ostream& out) const final;
		~Return() final = default;

		void acceptVisitor(PstVisitor& visitor) const override;
	};
}
