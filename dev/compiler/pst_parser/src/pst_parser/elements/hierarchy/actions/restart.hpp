#pragma once

#include "../statements.hpp"

namespace pst {
	class Restart final: public Action {
	public:
		explicit Restart(const dia::SourcePosition& position): Action(position) {}

		void dprint(std::ostream& out) const final;
		~Restart() final = default;

		void acceptVisitor(PstVisitor& visitor) const override;
	};
}
