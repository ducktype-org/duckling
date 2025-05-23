#pragma once

#include "../statements.hpp"

namespace pst {
	class Return final: public Action {
	public:
		explicit Return(const dia::SourcePosition& position): Action(position) {}

		void dprint(std::ostream& out) const final;
		~Return() final = default;

		void acceptVisitor(PstVisitor& visitor) const override;
	};
}
