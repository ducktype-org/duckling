#pragma once

#include "../statements/action.hpp"

namespace pst {
	class Restart final: public Action {
	public:
		explicit Restart(const LangParserState& state): Action(state) {}

		void dprint(std::ostream& out) const final;
		~Restart() final = default;

		void acceptVisitor(PstVisitor& visitor) const override;
	};
}
