// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

/**
 * @file export_keywords.hpp
 * @brief LSP Interface
 * @TODO: #3604 bring this back
 */

#pragma once

#include <string>

namespace lsp {
	/**
	 * @brief Class to export keywords for LSP purposes in JSON format.
	 *
	 * Member methods return JSON strings containing Keywords, Specials and/or Operators.
	 */
	class ExportKeywords {
	public:
		ExportKeywords();
		[[nodiscard]]
		std::string getKeywordListJson() const;
		[[nodiscard]]
		std::string getSpecialListJson() const;
		[[nodiscard]]
		std::string getOperatorListJson() const;
		[[nodiscard]]
		std::string getAllJson() const;
	};
}
