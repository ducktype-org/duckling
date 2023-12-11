#pragma once

#include <array>
#include <unicode/uniset.h>

namespace lexer {

	using UChar = UChar32;

	/**
	 * @brief Sets of characters used to distinguish between their usage in source code
	 * 
	 * @todo Maybe move to a more general location
	 * 
	 * @attention There are currently two codepoints that belong in both name_start and operator_continue They are U+1885 and U+1886 for now they will be treated as a continuation of the operator when it's ambiguous
	 * 
	 * @note There are some undefined characters in Syntax and operator sets(They will be caught by the decoder)
	 */
	struct Classifications {

		inline static icu::UnicodeSet name_start; ///< set of codepoints indicating a start of an identifier or a keyword
		inline static icu::UnicodeSet name_continue; ///< set of codepoints continuing an identifier or a keyword

		inline static icu::UnicodeSet operator_start; ///< set of codepoints indicating a start of an operator
		inline static icu::UnicodeSet operator_continue; ///< set of codepoints continuing an operator

		inline static icu::UnicodeSet vertical_space; ///< set of codepoints indicating a vertical space
		/**
		 * @brief set of codepoints indicating a newline. 
		 * 
		 * CR+LF is to be treated as one newline. This is especially important for escaping newline in strings if we choose to do so
		 */
		inline static icu::UnicodeSet newline;
		inline static icu::UnicodeSet whitespace; ///< union of vertical_space, newline and format_control
		/**
		 * @brief  Codepoints that allow to change the look of the text like left to right and right to left. They are to be treated according to @link https://unicode.org/reports/tr31/#Contexts_for_Ignorable_Format_Controls @endlink
		 * 
		 */
		inline static icu::UnicodeSet format_control;

		inline static icu::UnicodeSet special; ///< set of codepoints that have special meaning in code

		inline static icu::UnicodeSet syntax; ///< set of codepoints that have meaning outside of text. They start operators or have meaning in code(commas, brackets, semicolons,...)

		inline static icu::UnicodeSet open_bracket;
		inline static icu::UnicodeSet close_bracket;

		/** 
		 * @brief Custom codepoint(\U0010FFFF) signaling EOF internally
		 * 
		 * @note This codepoint is in a private use plane(It's left empty by the Unicode standard)
		 */
		inline static icu::UnicodeSet end_of_file;
		inline static UChar32 end_of_file_value = 0x10FFFF;

		inline static const std::array<icu::UnicodeSet*, 13> classes= {&name_start, &name_continue, &operator_start, &operator_continue, &vertical_space, &newline, &whitespace, &format_control, &special, &syntax, &open_bracket, &close_bracket, &end_of_file};

		static void init();
		Classifications() = delete;
	};
}