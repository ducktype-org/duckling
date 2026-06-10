/**
 * @file config.hpp
 * @brief Configuration for the Duckling source formatter.
 */
#pragma once

#include <base/types/ints.hpp>

#include <nlohmann/json_fwd.hpp>

namespace formatter {

	/**
	 * @brief Whether indentation is rendered with tab characters or spaces.
	 */
	enum class IndentStyle {
		Tab,
		Space,
	};

	/**
	 * @brief User-configurable formatter options.
	 *
	 * Defaults are chosen to match the prevailing style of the existing Duckling
	 * sources (tab indentation, operators surrounded by spaces).
	 */
	struct FormatConfig final {
		/** Indent with tabs or spaces. */
		IndentStyle indent_style = IndentStyle::Tab;

		/** Number of spaces per indentation level (only used when indent_style == Space). */
		u32 indent_width = 4;

		/**
		 * Soft target for line length. Over-long breakable constructs wrap: bracket groups
		 * with top-level commas explode one element per line, expressions break at method-chain
		 * dots or binary operators, and line comments re-flow onto continuation `//` lines.
		 * Unbreakable content (e.g. a single long literal) may still exceed the limit.
		 */
		u32 max_line_length = 100;

		/** Whether binary operators are surrounded by spaces (e.g. `a + b` vs `a+b`). */
		bool space_around_operators = true;

		/**
		 * @brief Returns the built-in default configuration.
		 */
		[[nodiscard]]
		static FormatConfig defaults() {
			return {};
		}

		/**
		 * @brief Builds a configuration from a JSON object.
		 *
		 * Unknown keys are ignored; missing keys keep their default value.
		 * Recognized keys:
		 *   - "indentStyle": "tab" | "space"
		 *   - "indentWidth": u32
		 *   - "maxLineLength": u32
		 *   - "spaceAroundOperators": bool
		 *
		 * @throws nlohmann::json::exception if a present key has the wrong type.
		 */
		[[nodiscard]]
		static FormatConfig fromJson(const nlohmann::json& json);
	};
}
