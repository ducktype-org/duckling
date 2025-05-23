#pragma once

#include "../statements.hpp"

namespace pst {
	class Redo final: public Action {
	public:
		explicit Redo(const dia::SourcePosition& position): Action(position) {}

		void dprint(std::ostream& out) const final;
		~Redo() final = default;

		void acceptVisitor(PstVisitor& visitor) const override;
	};
}
