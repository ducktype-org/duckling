/**
 * @file export_keywords.cpp
 * @brief This file defines the ExportKeywords class, which provides functionality to export
 * keywords, specials, and operators in JSON format.
 */
#include <lsp_interface/export_keywords.hpp>
#include <lsp_interface/utils.hpp>

#include <lang_definitions/key_spec_op.hpp>

#include <map>
#include <string>
#include <vector>

namespace lsp {
	/**
	 * @brief Constructs an ExportKeywords object and initializes the key_spec_op module.
	 */
	ExportKeywords::ExportKeywords() { lang_def::key_spec_op::init(); }

	/**
	 * @brief Gets the list of keywords in JSON format.
	 *
	 * @return std::string The JSON array representation of the keywords.
	 */
	std::string ExportKeywords::getKeywordListJson() const {
		std::vector<std::string> keywords;

		/**
		 * This loop parses the output of Duckling compiler's getKeywords() function and adds the
		 * keywords to the list.
		 */
		for (lang_def::Keyword k: lang_def::getKeywords()) {
			std::string keyword = lang_def::keywordToStr(k).str();
			if (keyword.rfind("NotA", 0) == std::string::npos)
				keywords.push_back(base::strConcat("\"", lang_def::keywordToStr(k).str(), "\""));
		}

		return jsonList(keywords);
	}

	/**
	 * @brief Gets the list of specials in JSON format.
	 *
	 * @return std::string The JSON array representation of the specials.
	 */
	std::string ExportKeywords::getSpecialListJson() const {
		std::vector<std::string> specials;

		/**
		 * This loop parses the output of Duckling compiler's getSpecials() function and adds the
		 * specials to the list.
		 */
		for (lang_def::Special s: lang_def::getSpecials()) {
			std::string special = lang_def::specialToStr(s).str();
			if (special.rfind("NotA", 0) == std::string::npos)
				specials.push_back(base::strConcat("\"", lang_def::specialToStr(s).str(), "\""));
		}

		return jsonList(specials);
	}

	/**
	 * @brief Gets the list of operators in JSON format.
	 *
	 * @return std::string The JSON array representation of the operators.
	 */
	std::string ExportKeywords::getOperatorListJson() const {
		std::vector<std::string> operators;

		/**
		 * This loop parses the output of Duckling compiler's getOperators() function and adds the
		 * operators to the list.
		 */
		for (lang_def::NamedOperator o: lang_def::getOperators()) {
			std::string op = lang_def::operatorToStr(o).str();
			if (op.rfind("NotA", 0) == std::string::npos)
				operators.push_back(base::strConcat("\"", lang_def::operatorToStr(o).str(), "\""));
		}

		return jsonList(operators);
	}

	/**
	 * @brief Gets all keywords, specials, and operators in JSON format.
	 *
	 * @return std::string The JSON object representation of the keywords, specials, and operators.
	 */
	std::string ExportKeywords::getAllJson() const {
		std::map<std::string, std::string> result;
		result["keywords"]  = getKeywordListJson();
		result["specials"]  = getSpecialListJson();
		result["operators"] = getOperatorListJson();

		return jsonDict(result);
	}
}
