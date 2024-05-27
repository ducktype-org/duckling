#include "elements.hpp"

namespace compiler::helios::code {

	void addIndent(usize indent, std::string& out) {
		out.append(indent * 4, ' ');
	}

	void ReturnStmt::debugPrint(usize indent, std::string& out) const {
		addIndent(indent, out);
		out += "return [@TODO]\n";
	}

	void VReturnStmt::debugPrint(usize indent, std::string& out) const {
		addIndent(indent, out);
		out += "void return\n";
	}
}
