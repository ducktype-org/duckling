#include "config.hpp"

#include <nlohmann/json.hpp>

namespace formatter {

	NLOHMANN_JSON_SERIALIZE_ENUM(
		IndentStyle,
		{
			{ IndentStyle::Tab, "tab" },
			{ IndentStyle::Space, "space" },
		}
	)

	/**
	 * @brief nlohmann ADL hook: deserializes a FormatConfig from a JSON object.
	 *
	 * Each field falls back to the value already in @p config (the defaults, since get<T>()
	 * default-constructs the target), so missing keys keep their default and unknown keys are
	 * ignored. A present key with the wrong type throws nlohmann::json::type_error.
	 */
	void from_json(  // NOLINT(readability-identifier-naming)
		const nlohmann::json& json, FormatConfig& config
	) {
		config.indent_style    = json.value("indentStyle", config.indent_style);
		config.indent_width    = json.value("indentWidth", config.indent_width);
		config.max_line_length = json.value("maxLineLength", config.max_line_length);
		config.space_around_operators =
			json.value("spaceAroundOperators", config.space_around_operators);
	}

	FormatConfig FormatConfig::fromJson(const nlohmann::json& json) {
		return json.get<FormatConfig>();
	}
}
