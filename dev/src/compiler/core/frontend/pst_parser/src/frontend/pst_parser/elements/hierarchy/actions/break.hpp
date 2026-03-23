#pragma once

#include "../statements/action.hpp"

namespace pst {
	class Break final: public Action {
	public:
		explicit Break(const LangParserState& state): Action(state) {}

		void dprint(std::ostream& out) const final;
		~Break() final = default;

		void acceptVisitor(PstVisitor& visitor) const override;
	};
}
