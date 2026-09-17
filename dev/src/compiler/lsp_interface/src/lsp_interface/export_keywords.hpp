/**
 * @file export_keywords.hpp
 * @brief LSP Interface
 * @note Currently unused.
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
