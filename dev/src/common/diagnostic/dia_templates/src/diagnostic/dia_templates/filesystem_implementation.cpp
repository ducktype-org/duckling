// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

/**
 * @file filesystem_implementation.cpp
 * @author Wojciech Rzepliński
 * @warning This is only one of the two alternative implementations of dia_templates.
 * One uses the embedding of the templates into the binary, this one uses the filesystem.
 */

#include "templates.hpp"

#include <base/misc/int_conv.hpp>

#include <filesystem>
#include <fstream>
#include <mutex>
#include <unordered_map>

namespace dia::templates {
	std::optional<std::string_view> loadTemplateFromPath(std::string_view path) {
		static std::unordered_map<std::string, std::string> cache;
		static std::mutex                                   mutex;

		std::lock_guard lock(mutex);

		// Check if the template is already loaded
		if (auto it = cache.find(std::string(path)); it != cache.end()) return it->second;

		namespace fs           = std::filesystem;
		fs::path root          = CMAKE_CURRENT_SOURCE_DIR;
		fs::path template_path = root / "templates" / path;

		// Try to find the file with extensions if it doesn't exist
		if (!fs::exists(template_path)) {
			if (fs::exists(template_path.string() + ".yaml"))
				template_path += ".yaml";
			else if (fs::exists(template_path.string() + ".yml"))
				template_path += ".yml";
			else
				return std::nullopt;
		}

		std::ifstream file(template_path, std::ios::binary | std::ios::ate);
		if (!file) return std::nullopt;

		auto        size = file.tellg();
		std::string content(static_cast<usize>(size), '\0');
		file.seekg(0);
		if (file.read(&content[0], size)) {
			auto [it, _] = cache.emplace(std::string(path), std::move(content));
			return it->second;
		}

		return std::nullopt;
	}
}
