/**
 * @file export_keywords.cpp
 * @brief This file defines the ExportKeywords class, which provides functionality to export keywords, specials, and operators in JSON format.
 */
#include "export_keywords.hpp"
#include <rift_definitions/key_spec_op.hpp>
#include <string>
#include <vector>
#include <map>

namespace {
	/**
     * @brief Converts a list of strings to a JSON array format.
     *
     * @param list The list of strings to convert.
     * @return std::string The JSON array representation of the list.
     */
	std::string jsonList(const std::vector<std::string>& list);
	
	/**
     * @brief Converts a dictionary of strings to a JSON object format.
     *
     * @param dict The dictionary of strings to convert.
     * @return std::string The JSON object representation of the dictionary.
     */
	std::string jsonDict(const std::map<std::string, std::string>& dict);
}

namespace lsp {
	/**
     * @brief Constructs an ExportKeywords object and initializes the key_spec_op module.
     */
	ExportKeywords::ExportKeywords() { rift_def::key_spec_op::init(); }

    /**
     * @brief Gets the list of keywords in JSON format.
     *
     * @return std::string The JSON array representation of the keywords.
     */
	std::string ExportKeywords::getKeywordListJson() const {
		std::vector<std::string> keywords;

		/**
		 * This loop parses the output of Duckling cvompiler's getKeywords() function and adds the keywords to the list.
		 */
		for (rift_def::Keyword k: rift_def::getKeywords()) {
			std::string keyword = rift_def::keywordToStr(k).str();
			if (keyword.rfind("NotA", 0) == std::string::npos)
				keywords.push_back(rift_def::keywordToStr(k).str());
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
		 * This loop parses the output of Duckling cvompiler's getSpecials() function and adds the specials to the list.
		 */
		for (rift_def::Special s: rift_def::getSpecials()) {
			std::string special = rift_def::specialToStr(s).str();
			if (special.rfind("NotA", 0) == std::string::npos)
				specials.push_back(rift_def::specialToStr(s).str());
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
		 * This loop parses the output of Duckling cvompiler's getOperators() function and adds the operators to the list.
		 */
		for (rift_def::Operator o: rift_def::getOperators()) {
			std::string op = rift_def::operatorToStr(o).str();
			if (op.rfind("NotA", 0) == std::string::npos)
				operators.push_back(rift_def::operatorToStr(o).str());
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

namespace {
	/**
     * @brief Converts a list of strings to a JSON array format.
     *
     * @param list The list of strings to convert.
     * @return std::string The JSON array representation of the list.
     */
	std::string jsonList(const std::vector<std::string>& list) {
		std::string result = "[";
		for (const std::string& str: list) result += "\"" + str + "\",";
		if (result[result.length() - 1] == ',') result.pop_back();
		result += "]";

		return result;
	}

    /**
     * @brief Converts a dictionary of strings to a JSON object format.
     *
     * @param dict The dictionary of strings to convert.
     * @return std::string The JSON object representation of the dictionary.
     */
	std::string jsonDict(const std::map<std::string, std::string>& dict) {
		std::string result = "{";
		for (const auto& pair: dict) {
			const auto& key   = pair.first;
			const auto& value = pair.second;
			result += "\"";
			result += key;
			result += "\":";
			result += value;
			result += ",";
		}
		if (result[result.length() - 1] == ',') result.pop_back();
		result += "}";

		return result;
	}
}
