#include "automatic.hpp"

namespace tpc {
	void identifierDprint(base::StrId value, std::ostream& out) {
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

	void nullAwareDprint(OptionalIdentifier ident, std::ostream& out) {
		if (ident.value.has_value())
			identifierDprint(ident.value.value(), out);
		else
			out << "\"<ANONYMOUS>\"";
	}
}
