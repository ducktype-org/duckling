#pragma once

#include "export_keywords.hpp"
#include <rift_definitions/key_spec_op.hpp>
#include <base/borrow_pointer.hpp>
#include <iostream>
#include <string>
#include <vector>
#include <map>

// make E a template
template<typename E>
E& operator++(E& e) {
	if (e == E::End) throw std::out_of_range("for E& operator ++ (E&)");
	e = E(static_cast<std::underlying_type<E>::type>(e) + 1);
	return e;
}

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

std::string get_keyword_list_json() {
	std::vector<std::string> keywords;
	for (rift_def::Keyword k: rift_def::getKeywords()) {
		keywords.push_back(rift_def::keywordToStr(k).str());
	}

	return json_list(keywords);
}

namespace lsp_interface {
	void print_keyword_list() { 
        rift_def::key_spec_op::init();
        std::cout << get_keyword_list_json() << "\n"; }
}
