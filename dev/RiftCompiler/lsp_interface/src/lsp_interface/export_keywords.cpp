#include "export_keywords.hpp"
#include <rift_definitions/key_spec_op.hpp>
#include <string>
#include <vector>
#include <map>

namespace {
	std::string jsonList(const std::vector<std::string>& list);
	std::string jsonDict(const std::map<std::string, std::string>& dict);
}

namespace lsp {
	ExportKeywords::ExportKeywords() { rift_def::key_spec_op::init(); }

	std::string ExportKeywords::getKeywordListJson() const {
		std::vector<std::string> keywords;
		for (rift_def::Keyword k: rift_def::getKeywords()) {
			std::string keyword = rift_def::keywordToStr(k).str();
			if (keyword.rfind("NotA", 0) == std::string::npos)
				keywords.push_back(rift_def::keywordToStr(k).str());
		}

		return jsonList(keywords);
	}

	std::string ExportKeywords::getSpecialListJson() const {
		std::vector<std::string> specials;
		for (rift_def::Special s: rift_def::getSpecials()) {
			std::string special = rift_def::specialToStr(s).str();
			if (special.rfind("NotA", 0) == std::string::npos)
				specials.push_back(rift_def::specialToStr(s).str());
		}

		return jsonList(specials);
	}

	std::string ExportKeywords::getOperatorListJson() const {
		std::vector<std::string> operators;
		for (rift_def::Operator o: rift_def::getOperators()) {
			std::string op = rift_def::operatorToStr(o).str();
			if (op.rfind("NotA", 0) == std::string::npos)
				operators.push_back(rift_def::operatorToStr(o).str());
		}

		return jsonList(operators);
	}

	std::string ExportKeywords::getAllJson() const {
		std::map<std::string, std::string> result;
		result["keywords"]  = getKeywordListJson();
		result["specials"]  = getSpecialListJson();
		result["operators"] = getOperatorListJson();

		return jsonDict(result);
	}
}

namespace {
	std::string jsonList(const std::vector<std::string>& list) {
		std::string result = "[";
		for (const std::string& str: list) result += "\"" + str + "\",";
		if (result[result.length() - 1] == ',') result.pop_back();
		result += "]";

		return result;
	}

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
