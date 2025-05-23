#pragma once

#include "../statements/action.hpp"

namespace pst {
	class Continue final: public Action {
	public:
		explicit Continue(const dia::SourcePosition& position): Action(position) {}

		void dprint(std::ostream& out) const final;
		~Continue() final = default;

		void acceptVisitor(PstVisitor& visitor) const override;
	};
}
