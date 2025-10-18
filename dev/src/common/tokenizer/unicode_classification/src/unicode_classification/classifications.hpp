#pragma once

#include <unicode/uniset.h>

#include <array>

namespace unicode {

	/**
	 * @brief Sets of characters used to decide what a certain character means in source code
	 *
	 * @note There are currently two codepoints that belong in both `name_start` and
	 * `operator_continue`. They are U+1885 and U+1886. For now they will be treated as a
	 * continuation of the operator when it's ambiguous.
	 *
	 * @note There are some undefined characters in Syntax and operator sets(They are currently
	 * detected by the decoder).
	 */
	struct Classifications final {
		static icu::UnicodeSet
			name_start;     ///< set of codepoints indicating a start of an identifier or a keyword
		static icu::UnicodeSet
			name_continue;  ///< set of codepoints continuing an identifier or a keyword

		static icu::UnicodeSet
			operator_start;  ///< set of codepoints indicating a start of an operator
		static icu::UnicodeSet operator_continue;  ///< set of codepoints continuing an operator

		static icu::UnicodeSet vertical_space;  ///< set of codepoints indicating a vertical space
		/**
		 * @brief set of codepoints indicating a newline.
		 *
		 * CR+LF is to be treated as one newline. This is especially important for escaping newline
		 * in strings if we choose to do so
		 */
		static icu::UnicodeSet newline;
		static icu::UnicodeSet whitespace;  ///< union of vertical_space, newline and format_control
		/**
		 * @brief  Codepoints that allow to change the look of the text like left to right and right
		 * to left. They are to be treated according to the [Unicode
		 * report](https://unicode.org/reports/tr31/#Contexts_for_Ignorable_Format_Controls).
		 *
		 */
		static icu::UnicodeSet format_control;

		static icu::UnicodeSet special;  ///< set of codepoints that have special meaning in code

		static icu::UnicodeSet
			syntax;  ///< set of codepoints that have meaning outside of text. They start operators
		             ///< or have meaning in code (commas, brackets, semicolons,...)

		static icu::UnicodeSet open_bracket;
		static icu::UnicodeSet close_bracket;

		/**
		 * @brief Custom codepoint(\U0010FFFF) signaling EOF internally
		 *
		 * @note This codepoint is in a private use plane(It's left empty by the Unicode standard)
		 */
		static icu::UnicodeSet   end_of_file;
		static constexpr UChar32 END_OF_FILE_VALUE = 0x10'FF'FF;

		// NOLINTBEGIN(cppcoreguidelines-interfaces-global-init)
		inline static const std::array<icu::UnicodeSet*, 13> CLASSES
			= { &name_start,   &name_continue, &operator_start, &operator_continue, &vertical_space,
			    &newline,      &whitespace,    &format_control, &special,           &syntax,
			    &open_bracket, &close_bracket, &end_of_file };
		// NOLINTEND(cppcoreguidelines-interfaces-global-init)

		/**
		 * @brief Populates the data members of this class.
		 * It will be called automagically when InitObject is used.
		 */
		static void init();
		Classifications() = delete;
	};
}
