#pragma once

#include "export_keywords.hpp"
#include <rift_definitions/key_spec_op.hpp>
#include <base/borrow_pointer.hpp>
#include <iostream>
#include <string>
#include <vector>
#include <map>

std::string json_list(const std::vector<std::string>& list) {
	std::string result = "[";
	for (const std::string& str: list) result += "\"" + str + "\",";
	if (result[result.length() - 1] == ',') result.pop_back();
	result += "]";
	return result;
}

std::string json_dict(const std::map<std::string, std::string>& dict) {
	std::string result = "{";
	for (const auto& pair: dict) {
		const auto& key   = pair.first;
		const auto& value = pair.second;
		result += "\"" + key + "\":\"" + value + "\",";
	}
	if (result[result.length() - 1] == ',') result.pop_back();
	result += "}";
	return result;
}

std::map<std::string, std::string> get_all_map();
namespace lsp_interface {

	std::string get_keyword_list_json() {
		rift_def::key_spec_op::init();
		std::vector<std::string> keywords;
		for (rift_def::Keyword k: rift_def::getKeywords())
			keywords.push_back(rift_def::keywordToStr(k).str());

		return json_list(keywords);
	}

	std::string get_special_list_json() {
		rift_def::key_spec_op::init();
		std::vector<std::string> specials;
		for (rift_def::Special s: rift_def::getSpecials())
			specials.push_back(rift_def::specialToStr(s).str());

		return json_list(specials);
	}

	std::string get_operator_list_json() {
		rift_def::key_spec_op::init();
		std::vector<std::string> operators;
		for (rift_def::Operator o: rift_def::getOperators())
			operators.push_back(rift_def::operatorToStr(o).str());

		return json_list(operators);
	}

    std::string get_all_json() {
		rift_def::key_spec_op::init();
        return json_dict(get_all_map());
    }

	void print_keyword_list() {
		rift_def::key_spec_op::init();
		std::cout << get_keyword_list_json() << "\n";
	}

	void print_special_list() {
		rift_def::key_spec_op::init();
		std::cout << get_special_list_json() << "\n";
	}

	void print_operator_list() {
		rift_def::key_spec_op::init();
		std::cout << get_operator_list_json() << "\n";
	}
    
    void print_all_dict() {
        rift_def::key_spec_op::init();
        std::cout << get_all_json() << "\n";
    }
}

std::map<std::string, std::string> get_all_map() {
	std::map<std::string, std::string> result;
	result["keywords"]  = lsp_interface::get_keyword_list_json();
	result["specials"]  = lsp_interface::get_special_list_json();
	result["operators"] = lsp_interface::get_operator_list_json();
    return result;
}