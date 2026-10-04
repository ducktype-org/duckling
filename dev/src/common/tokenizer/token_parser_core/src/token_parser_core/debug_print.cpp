#include "debug_print.hpp"

namespace tpc {
	void identifierDprint(base::StrID value, std::ostream& out) {
		// @TODO: change Name to Identifier
		out << "{\"Name\": ";
		if (value.isBad())
			out << "\"BAD_NAME\"";
		else
			out << "\"" << value.strView() << "\"";
		out << "}";
	}

	void nullAwareDprint(Identifier ident, std::ostream& out) {
		identifierDprint(ident.value, out);
	}

	void nullAwareDprint(Keyword key, std::ostream& out) {
		out << "\"" << keywordToStr(key).str() << "\"";
	}

	void nullAwareDprint(Special spec, std::ostream& out) {
		out << "\"" << specialToStr(spec).str() << "\"";
	}

	void nullAwareDprint(Operator op, std::ostream& out) { out << "\"" << op.str() << "\""; }
}
