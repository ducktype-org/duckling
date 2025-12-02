#include "styles.hpp"

#include <base/except/exceptions.hpp>

#include <utility>

namespace term_ui {
	bool use_color = true;

	const Style ERROR_STYLE(
		"error", "E", true, rang::fg::red, rang::style::bold, rang::style::bold, '^', '|', 'Y'
	);

	const Style WARNING_STYLE(
		"warning", "W", true, rang::fg::yellow, rang::style::bold, rang::style::bold, '^', '|', 'Y'
	);
	const Style NOTE_STYLE(
		"note", "N", false, rang::fg::blue, rang::style::bold, rang::style::reset, '~', '|', 'v'
	);
	const Style HINT_STYLE(
		"hint", "H", false, rang::fg::green, rang::style::bold, rang::style::reset, '+', '|', '+'
	);
	const Style DOCS_STYLE(
		"docs", "D", false, rang::fg::cyan, rang::style::bold, rang::style::reset, '~', '|', '?'
	);

	const Style LINE_START_STYLE(
		"", "", false, rang::fg::reset, rang::style::bold, rang::style::reset
	);

	Style::Style(
		std::string name,
		std::string prefix,
		bool        print_id,
		rang::fg    color,
		rang::style style,
		rang::style main_text_style,
		char        underline_char,
		char        lowering_char,
		char        lowering_attach_char
	):
		  NAME(std::move(name)),
		  PREFIX(std::move(prefix)),
		  PRINT_ID(print_id),
		  COLOR(color),
		  STYLE(style),
		  MAIN_TEXT_STYLE(main_text_style),
		  UNDERLINE_CHAR(underline_char),
		  LOWERING_CHAR(lowering_char),
		  LOWERING_ATTACH_CHAR(lowering_attach_char) {}

	void Style::prepare(std::ostream& out) const {
		if (use_color) out << COLOR;
		out << STYLE;
	}

	void Style::printWith(const std::string& text, std::ostream& out) const {
		prepare(out);
		out << text;
		resetStyles(out);
	}

	void Style::prepareMainText(std::ostream& out) const { out << MAIN_TEXT_STYLE; }

	void Style::printMainWith(const std::string& text, std::ostream& out) const {
		prepareMainText(out);
		out << text;
		resetStyles(out);
	}

	void Style::printName(u64 id, std::ostream& out) const {
		prepare(out);
		out << NAME;
		if (PRINT_ID) out << '[' << PREFIX << id << ']';
		resetStyles(out);
	}

	Style getStyleFromType(StyleType type) {
		switch (type) {
		case StyleType::Error:
			return ERROR_STYLE;
		case StyleType::Warning:
			return WARNING_STYLE;
		case StyleType::Note:
			return NOTE_STYLE;
		case StyleType::Hint:
			return HINT_STYLE;
		case StyleType::Docs:
			return DOCS_STYLE;
		}
		CORE_ASSERT(false, "Unknown info type.");
		return ERROR_STYLE;
	}

	void resetStyles(std::ostream& out) { out << rang::fg::reset << rang::style::reset; }

	void printLineStart(u64 tab_space, u64 line_no, std::ostream& out) {
		std::string line_no_str = std::to_string(line_no);
		std::string line_start =
			// Print line number.
			line_no_str +
			// Fill the whitespace before the line bar.
			std::string(tab_space - std::to_string(line_no).size(), ' ') +
			// Print the line bar.
			"| ";

		LINE_START_STYLE.printWith(line_start, out);
	}

	void printLineStart(u64 tab_space, std::ostream& out) {
		// Print tab space and line bar.
		LINE_START_STYLE.printWith(std::string(tab_space, ' ') + "| ", out);
	}
}
