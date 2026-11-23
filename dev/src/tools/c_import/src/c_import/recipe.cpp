// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "recipe.hpp"

#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <array>
#include <format>
#include <utility>

namespace c_import {

	namespace {

		constexpr std::string_view RECIPE_HEADER
			= "# Recipe of `duckbind`: how the bindings in `src/generated/` are made from C "
			  "headers.\n"
			  "# Edit it and run `duckbind regen` to update them. Paths are relative to this "
			  "file.\n";

		/// The recipe's keys, in the order they are written.
		std::array<std::pair<std::string_view, std::vector<std::string> Recipe::*>, 6> keys() {
			return { {
				{ "headers", &Recipe::headers },
				{ "pkg-config", &Recipe::pkg_config },
				{ "cflags", &Recipe::cflags },
				{ "libraries", &Recipe::libraries },
				{ "include", &Recipe::include },
				{ "exclude", &Recipe::exclude },
			} };
		}

		std::filesystem::path normalized(const std::filesystem::path& path) {
			return std::filesystem::absolute(path).lexically_normal();
		}

	}

	std::expected<Recipe, std::string> parseRecipe(std::string_view yaml) {
		try {
			auto root = YAML::Load(std::string{ yaml });
			if (!root.IsMap()) return std::unexpected("the recipe is not a mapping");
			Recipe     recipe;
			const auto all = keys();
			for (const auto& entry: root) {
				auto key   = entry.first.as<std::string>();
				auto known = std::ranges::find(all, key, [](const auto& k) { return k.first; });
				if (known == all.end())
					return std::unexpected(std::format("unknown key `{}`", key));
				if (!entry.second.IsSequence())
					return std::unexpected(std::format("`{}` is not a list", key));
				recipe.*(known->second) = entry.second.as<std::vector<std::string>>();
			}
			if (recipe.headers.empty()) return std::unexpected("`headers` is empty");
			return recipe;
		} catch (const YAML::Exception& error) { return std::unexpected(error.what()); }
	}

	std::string recipeText(const Recipe& recipe) {
		YAML::Emitter out;
		out << YAML::BeginMap;
		for (const auto& [key, member]: keys()) {
			const auto& values = recipe.*member;
			if (values.empty()) continue;
			out << YAML::Key << std::string{ key } << YAML::Value << YAML::BeginSeq;
			for (const auto& value: values) out << YAML::DoubleQuoted << value;
			out << YAML::EndSeq;
		}
		out << YAML::EndMap;
		return std::format("{}{}\n", RECIPE_HEADER, out.c_str());
	}

	void relativize(
		Recipe& recipe, const std::filesystem::path& cwd, const std::filesystem::path& root
	) {
		std::error_code ec;
		for (auto& header: recipe.headers)
			if (std::filesystem::is_regular_file(cwd / header, ec))
				header = relativePath(cwd / header, root);
		for (auto& flag: recipe.cflags)
			if (flag.starts_with("-I")) flag = "-I" + relativePath(cwd / flag.substr(2), root);
		for (auto& library: recipe.libraries)
			if (library.starts_with("-L"))
				library = "-L" + relativePath(cwd / library.substr(2), root);
			else if (!library.starts_with('-'))
				library = relativePath(cwd / library, root);
	}

	std::string relativePath(const std::filesystem::path& path, const std::filesystem::path& base) {
		auto relative = normalized(path).lexically_relative(normalized(base)).generic_string();
		if (relative.ends_with('/')) relative.pop_back();
		return relative.empty() ? "." : relative;
	}

}
