#include "elements_implementation.hpp"
#include "pst_parser/pst_visitor.hpp"

namespace pst {
	bool Decl::trailingSemicolon() { return false; }

	void Decl::acceptVisitor(PstStmtVisitor& visitor) const {
		throw base::NotYetImplemented(
			"Called visit on pst::Decl's subclass, that does not support visiting."
		);
	}
}
