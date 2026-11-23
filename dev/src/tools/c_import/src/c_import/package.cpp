// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "package.hpp"

#include <yaml-cpp/yaml.h>

#include <array>
#include <format>
#include <fstream>
#include <sstream>
#include <utility>

namespace c_import {

	namespace {

		std::array<std::pair<std::string_view, const std::vector<std::string>*>, 2> libraryKeys(
			const Resolved& resolved
		) {
			return { {
				{ "external-libs", &resolved.external_libs },
				{ "external-lib-paths", &resolved.external_lib_paths },
			} };
		}

		std::string flowList(const std::vector<std::string>& values) {
			std::string text = "[";
			for (const auto& value: values) text += (text.size() > 1 ? ", " : "") + value;
			return text + "]";
		}

		std::expected<YAML::Node, std::string> metadataOf(std::string_view manifest) {
			try {
				auto metadata = YAML::Load(std::string{ manifest })["metadata"];
				if (!metadata.IsMap())
					return std::unexpected("the manifest has no `metadata` table");
				return metadata;
			} catch (const YAML::Exception& error) { return std::unexpected(error.what()); }
		}

		std::vector<std::string> splitLines(std::string_view text) {
			std::vector<std::string> lines;
			std::istringstream       in{ std::string{ text } };
			for (std::string line; std::getline(in, line);) lines.push_back(line);
			return lines;
		}

		bool isBlank(std::string_view line) {
			return line.find_first_not_of(" \t") == std::string_view::npos;
		}

	}

	std::expected<std::string, std::string> manifestName(std::string_view manifest) {
		auto metadata = metadataOf(manifest);
		if (!metadata) return std::unexpected(metadata.error());
		if (!(*metadata)["name"].IsScalar())
			return std::unexpected("the manifest has no `metadata.name`");
		return (*metadata)["name"].as<std::string>();
	}

	std::expected<ManifestLibraries, std::string> manifestLibraries(std::string_view manifest) {
		auto metadata = metadataOf(manifest);
		if (!metadata) return std::unexpected(metadata.error());
		ManifestLibraries libraries;
		try {
			if (auto libs = (*metadata)["external-libs"])
				libraries.external_libs = libs.as<std::vector<std::string>>();
			if (auto paths = (*metadata)["external-lib-paths"])
				libraries.external_lib_paths = paths.as<std::vector<std::string>>();
		} catch (const YAML::Exception& error) { return std::unexpected(error.what()); }
		return libraries;
	}

	std::string newManifest(
		std::string_view name, std::string_view version, const Resolved& resolved
	) {
		auto text = std::format("metadata:\n  name: {}\n  version: '{}'\n", name, version);
		for (const auto& [key, values]: libraryKeys(resolved))
			if (!values->empty()) text += std::format("  {}: {}\n", key, flowList(*values));
		return text;
	}

	std::expected<std::string, std::string> withLibraryKeys(
		std::string_view manifest, const Resolved& resolved
	) {
		auto lines = splitLines(manifest);
		auto start = std::ranges::find_if(lines, [](const std::string& line) {
			return line.starts_with("metadata:");
		});
		if (start == lines.end()) return std::unexpected("the manifest has no `metadata` table");
		auto begin = static_cast<std::size_t>(start - lines.begin()) + 1;
		auto end   = begin;
		while (end < lines.size()
		       && (lines[end].empty() || lines[end].front() == ' ' || lines[end].front() == '#'))
			++end;

		std::string indent = "  ";
		for (auto i = begin; i < end; ++i) {
			auto at = lines[i].find_first_not_of(' ');
			if (at != std::string::npos && lines[i][at] != '#') {
				indent = lines[i].substr(0, at);
				break;
			}
		}

		for (const auto& [key, values]: libraryKeys(resolved)) {
			const auto prefix = std::format("{}{}:", indent, key);
			auto       found  = end;
			for (auto i = begin; i < end; ++i)
				if (lines[i].starts_with(prefix)) found = i;
			const auto line = std::format("{}{}: {}", indent, key, flowList(*values));
			if (found == end) {
				if (values->empty()) continue;
				auto last = end;
				// Before trailing blank lines and comments, which belong to what follows.
				while (last > begin && (isBlank(lines[last - 1]) || lines[last - 1].front() == '#'))
					--last;
				lines.insert(lines.begin() + static_cast<std::ptrdiff_t>(last), line);
				++end;
				continue;
			}
			std::string_view value = lines[found];
			value.remove_prefix(prefix.size());
			value.remove_prefix(std::min(value.find_first_not_of(' '), value.size()));
			if (!value.starts_with('[') || !value.contains(']'))
				return std::unexpected(std::format(
					"`metadata.{}` is not a one-line `[...]` list; set it to {} by hand",
					key,
					flowList(*values)
				));
			if (values->empty()) {
				lines.erase(lines.begin() + static_cast<std::ptrdiff_t>(found));
				--end;
			} else {
				lines[found] = line;
			}
		}

		std::string text;
		for (const auto& line: lines) text += line + "\n";
		return text;
	}

	std::string rootModule(std::string_view name) {
		return std::format(
			"# Re-exports the generated bindings, so `import {0}.*;` reaches them. This file is "
			"never\n# regenerated: helpers written here or in other files next to it are kept.\n"
			"import {0}.{1}.{0}.*;\n",
			name,
			GENERATED_DIR
		);
	}

	std::string generatedRootModule(const std::vector<std::string>& banner) {
		std::string text;
		for (const auto& line: banner) text += std::format("# {}\n", line);
		return text;
	}

	std::expected<std::string, std::string> readFile(const std::filesystem::path& path) {
		std::ifstream in(path, std::ios::binary);
		if (!in) return std::unexpected(std::format("cannot read `{}`", path.string()));
		std::stringstream text;
		text << in.rdbuf();
		return text.str();
	}

	std::expected<void, std::string> writeFile(
		const std::filesystem::path& path, std::string_view content
	) {
		std::error_code ec;
		if (path.has_parent_path()) std::filesystem::create_directories(path.parent_path(), ec);
		std::ofstream out(path, std::ios::binary | std::ios::trunc);
		out << content;
		if (!out) return std::unexpected(std::format("cannot write `{}`", path.string()));
		return {};
	}

}
