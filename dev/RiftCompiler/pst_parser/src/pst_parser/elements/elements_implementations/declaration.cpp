#include "elements_implementation.hpp"
#include "pst_parser/pst_visitor.hpp"

namespace pst {
	bool Decl::trailingSemicolon() { return false; }
	void Decl::acceptVistior(PstStmtVisitor& visitor) const { visitor.visitDecl(*this); }
}
