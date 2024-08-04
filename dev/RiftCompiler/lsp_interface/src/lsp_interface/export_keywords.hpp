/**
 * @file export_keywords.hpp
 * @brief LSP Interface
 */

#pragma once

#include <string>

namespace lsp {
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
