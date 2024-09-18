/**
 * @file export_keywords.hpp
 * @brief LSP Interface
 */

#pragma once

#include <string>

namespace lsp {
	/**
	 * @brief Class to export keywords for LSP purposes in JSON format.
	 *
	 * Member mrthods return JSON strings containing different types of keywords.
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
