#include "config.hpp"

#include <nlohmann/json.hpp>

#include <string>

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
}
