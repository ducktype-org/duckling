#pragma once

#include "../../meta.hpp"

namespace pst {
	/**
	 * @brief Dummy base element common to format string sub elements
	 */
	class FormatSubElement: public NotStmt {
	protected:
		explicit FormatSubElement(const LangParserState& state): NotStmt(state) {}
	};
}
