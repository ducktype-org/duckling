#include "export_keywords.hpp"
#include <rift_definitions/key_spec_op.hpp>
#include <string>
#include <vector>
#include <map>

std::string json_list(const std::vector<std::string>& list);
std::string json_dict(const std::map<std::string, std::string>& dict);

namespace lsp_interface {

	LspInterface::LspInterface() {
		rift_def::key_spec_op::init();
	}

	std::string LspInterface::get_keyword_list_json() {
		std::vector<std::string> keywords;
		for (rift_def::Keyword k : rift_def::getKeywords())
			keywords.push_back(rift_def::keywordToStr(k).str());

		return json_list(keywords);
	}

	std::string LspInterface::get_special_list_json() {
		std::vector<std::string> specials;
		for (rift_def::Special s : rift_def::getSpecials())
			specials.push_back(rift_def::specialToStr(s).str());

		return json_list(specials);
	}

	std::string LspInterface::get_operator_list_json() {
		std::vector<std::string> operators;
		for (rift_def::Operator o : rift_def::getOperators())
			operators.push_back(rift_def::operatorToStr(o).str());

		return json_list(operators);
	}

	std::string LspInterface::get_all_json() {
		std::map<std::string, std::string> result;
		result["keywords"] = get_keyword_list_json();
		result["specials"] = get_special_list_json();
		result["operators"] = get_operator_list_json();
		return json_dict(result);
	}
}

std::string json_list(const std::vector<std::string>& list) {
	std::string result = "[";
	for (const std::string& str : list) result += "\"" + str + "\",";
	if (result[result.length() - 1] == ',') {
		result.pop_back();
	}
	result += "]";
	return result;
}

std::string json_dict(const std::map<std::string, std::string>& dict) {
	std::string result = "{";
	for (const auto& pair : dict) {
		const auto& key = pair.first;
		const auto& value = pair.second;
		result += "\"" + key + "\":" + value + ",";
	}
	if (result[result.length() - 1] == ',') {
		result.pop_back();
	}
	result += "}";
	return result;
}
