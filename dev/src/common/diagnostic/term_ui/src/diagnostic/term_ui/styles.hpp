// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once
#include <rang.hpp>

#include <base/types/ints.hpp>

#include <diagnostic/core/term_ui_view.hpp>
#include <diagnostic/term_ui/module_flags/module_flags.hpp>

namespace term_ui {

	using StyleType = dia::term_ui_view::StyleType;

	/**
	 * @brief A class containing information about a specific style.
	 *
	 * A style may influence how the following parts of an info are displayed:
	 *
	 * - header message (both the introductory name, e.g. `error[E1010]`,
	 * and the actual content of the message),
	 *
	 * - pointer message,
	 *
	 * - utility characters used for positioning the pointer message
	 * and highlighting pieces of code.
	 */
	struct Style {
	private:
		/**
		 * @brief The style name. Displayed before the header message.
		 *
		 * E.g. `error` or `note`.
		 */
		const std::string name;

		/**
		 * @brief The style prefix.
		 *
		 * If `print_id` flag is on, it preceeds the displayed info code
		 * after the style name.
		 *
		 * E.g. `E` (for error) or `N` (for note).
		 */
		const std::string prefix;
		/**
		 * @brief Whether the info code should be displayed after
		 * the style name.
		 *
		 * If on, `printName` method displays both the style name
		 * and the concatenation of `prefix` and its provided info code, e.g.:
		 *
		 * `error[E1010]`
		 *
		 * for `name`=`"error"`, `prefix`=`"E"`, and `info_id`=`1010`.
		 *
		 * If off, `printName` method only displays the style name.
		 */
		const bool print_id;

		// The color associated with this style.
		const rang::fg color;

		// The boldness modifier associated with this style for all text
		// apart from the header message.
		const rang::style style;
		// The boldness modifier associated with this style for the header
		// message.
		const rang::style main_text_style;

	public:
		/**
		 * @brief The character used for highlighting segments of code.
		 *
		 * A series of chars of this type is displayed in the line below
		 * the highlighted segment, acting as a colorful underlining.
		 */
		const char underline_char;
		/**
		 * @brief The character used for lowering the pointer message
		 * with respect to the code line it refers to.
		 *
		 * If the pointer message does not fit closer to the code line
		 * (e.g. if other pointer messages block the view), this character
		 * is used to connect the lowered message with the code underlining.
		 */
		const char lowering_char;
		/**
		 * @brief The character used when a code underlining meets with
		 * a lowering character.
		 *
		 * This is a special character which, for clarity, replaces
		 * an underlining character in its place when a lowering char
		 * related to this underlining is present in the line below
		 * in the same column.
		 *
		 * E.g., for `underline_char`=`'^'`, `lowering_char`=`'|'`, and
		 * `LOWERING_ATTACH_CHAR`=`'Y'` we may have:
		 *
		 * ```
		 * underlined code
		 * Y^^^^^^^^^^^^^^
		 * |
		 * pointer message
		 * ```
		 */
		const char lowering_attach_char;

		// Create a style object by specifying all of its fields.
		Style(
			std::string name,
			std::string prefix,
			bool        print_id,
			rang::fg    color,
			rang::style style,
			rang::style main_text_style,
			char        underline_char       = ' ',
			char        lowering_char        = ' ',
			char        lowering_attach_char = ' '
		);

		/**
		 * @brief Print the given text in this style (colored).
		 *
		 * @param text The text to be displayed.
		 * @param out The output stream.
		 */
		void printWith(const std::string& text, std::ostream& out) const;

		/**
		 * @brief Print the given text in this style (non-colored).
		 *
		 * This method is used only for the content of the header message,
		 * as we want to keep it especially readable in the terminal window.
		 *
		 * @param text The text to be displayed.
		 * @param out The output stream.
		 */
		void printMainWith(const std::string& text, std::ostream& out) const;

		/**
		 * @brief Print the name associated with this style with the specified
		 * id if the style suggests it should be displayed.
		 *
		 * @param id The info id (*info code*). It may or may not be displayed,
		 * depending on the style's setup.
		 * @param out The output stream.
		 */
		void printName(u64 id, std::ostream& out) const;

	private:
		// Prepare the output stream to print colored text in this style.
		void prepare(std::ostream& out) const;
		// Prepare the output stream to print non-colored text in this style
		// (used for the content of the header message).
		void prepareMainText(std::ostream& out) const;
	};

	Style getStyleFromType(StyleType type);

	/**
	 * @brief Reset the current style of displayed text.
	 *
	 * @param out The output stream that is display onto.
	 */
	void resetStyles(std::ostream& out);

	/**
	 * @brief Print a start of a code line which has a number.
	 * E.g.:
	 *
	 * `12  |`
	 *
	 * @param tab_space The column in which the vertical bar should appear
	 * (note that the bar may be displayed in a further column if the line
	 * number is too long).
	 * @param line_no The number of this line.
	 * @param out The output stream.
	 */
	void printLineStart(u64 tab_space, u64 line_no, std::ostream& out);

	/**
	 * @brief Print a start of a code line which *does not* have a number.
	 * E.g.:
	 *
	 * `    |`
	 *
	 * @param tab_space The column in which the vertical bar should appear.
	 * @param out The output stream.
	 */
	void printLineStart(u64 tab_space, std::ostream& out);
}
