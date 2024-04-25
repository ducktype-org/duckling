/**
 * @file export_keywords.hpp
 * @brief LSP Interface
 */

#pragma once

#include <string>

namespace lsp {
	class LspInterface {
	public:
		LspInterface();
		std::string getKeywordListJson();
		std::string getSpecialListJson();
		std::string getOperatorListJson();
		std::string getAllJson();
	};
}
