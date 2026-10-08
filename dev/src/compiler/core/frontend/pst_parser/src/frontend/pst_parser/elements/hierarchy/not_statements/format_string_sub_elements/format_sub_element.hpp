// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

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
		explicit FormatSubElement(LangElementConstructionArgument state): NotStmt(state) {}

	public:
		ELEMENT_CLONE_DECL(FormatSubElement);
	};
}
