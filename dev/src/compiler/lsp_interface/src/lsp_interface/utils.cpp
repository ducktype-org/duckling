// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include <frontend/module_tree/module_tree.hpp>
#include <frontend/module_tree/queries.hpp>
#include <lsp_interface/utils.hpp>

#include <base/except/exceptions.hpp>
#include <base/str/str_utils.hpp>

#include <filesystem/file.hpp>
#include <filesystem/file_path.hpp>
#include <query_framework/entry/with_context_do.hpp>

#define ext_is_ok(ext) \
	ext == LANG_MODULE_FILE || ext == ".duckling" || ext == ".dl" || ext == LANG_SCRIPT_FILE

namespace lsp {
	std::string jsonList(const std::vector<std::string>& list) {
		std::string result = "[";
		for (const std::string& str: list) result += str + ",";
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
