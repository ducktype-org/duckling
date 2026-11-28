// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "config.hpp"

#include <nlohmann/json.hpp>

#include <string>
#include <system_error>
#include <utility>

namespace formatter {

	namespace {

		/**
		 * Reads the "indentStyle" key. Parsed by hand rather than through
		 * NLOHMANN_JSON_SERIALIZE_ENUM, whose generated from_json silently falls back to the
		 * first entry, so a typo like "tabs" would quietly mean tabs.
		 */
		IndentStyle readIndentStyle(const nlohmann::json& json, IndentStyle fallback) {
			if (not json.contains("indentStyle")) return fallback;
			const auto& value = json.at("indentStyle");
			if (value.is_string()) {
				const auto name = value.get<std::string>();
				if (name == "tab") return IndentStyle::Tab;
				if (name == "space") return IndentStyle::Space;
			}
			throw nlohmann::json::other_error::create(
				501, R"(indentStyle must be "tab" or "space")", &json
			);
		}
	}

	/**
	 * @brief nlohmann ADL hook: deserializes a FormatConfig from a JSON object.
	 *
	 * Each field falls back to the value already in @p config (the defaults, since get<T>()
	 * default-constructs the target), so missing keys keep their default and unknown keys are
	 * ignored. A present key with the wrong type or an unrecognized "indentStyle" throws
	 * nlohmann::json::exception.
	 */
	void from_json(  // NOLINT(readability-identifier-naming)
		const nlohmann::json& json,
		FormatConfig&         config
	) {
		config.indent_style    = readIndentStyle(json, config.indent_style);
		config.indent_width    = json.value("indentWidth", config.indent_width);
		config.max_line_length = json.value("maxLineLength", config.max_line_length);
		config.max_empty_lines = json.value("maxEmptyLines", config.max_empty_lines);
		config.space_around_operators
			= json.value("spaceAroundOperators", config.space_around_operators);
	}

	FormatConfig FormatConfig::fromJson(const nlohmann::json& json) {
		return json.get<FormatConfig>();
	}

	base::Optional<std::filesystem::path> findConfigFile(const std::filesystem::path& start_dir) {
		std::error_code error;
		auto            dir = std::filesystem::absolute(start_dir, error).lexically_normal();
		if (error) return {};
		// "a/b/" has an empty file name, and its parent_path() is "a/b" again.
		if (not dir.has_filename()) dir = dir.parent_path();

		while (true) {
			auto candidate = dir / CONFIG_FILE_NAME;
			if (std::filesystem::is_regular_file(candidate, error)) return candidate;

			auto parent = dir.parent_path();
			// The root is its own parent.
			if (parent == dir) return {};
			dir = std::move(parent);
		}
	}
}
