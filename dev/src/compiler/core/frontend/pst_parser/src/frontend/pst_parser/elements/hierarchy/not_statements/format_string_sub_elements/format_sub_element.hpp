#pragma once

#include "../../meta.hpp"

namespace pst {
	/**
	 * @brief Dummy base element common to format string sub elements
	 */
	class FormatSubElement: public NotStmt {
		THIS_CLASS(FormatSubElement);
		PARENT_CLASS(NotStmt);

	protected:
		explicit FormatSubElement(const LangParserState& state): NotStmt(state) {}

	public:
		ELEMENT_CLONE_DECL(FormatSubElement);
	};
}
