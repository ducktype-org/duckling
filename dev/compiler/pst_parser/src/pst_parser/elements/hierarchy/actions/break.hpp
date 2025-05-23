#pragma once

#include "../statements.hpp"

namespace pst {
	class Break final: public Action {
	public:
		explicit Break(const dia::SourcePosition& position): Action(position) {}

		void dprint(std::ostream& out) const final;
		~Break() final = default;

		void acceptVisitor(PstVisitor& visitor) const override;
	};
}
