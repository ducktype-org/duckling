#pragma once

#include "code_line.hpp"
#include "highlight.hpp"
#include "line.hpp"
#include "styles.hpp"

#include <proto/view.pb.h>

#include <base/maps.hpp>

#include <iostream>
#include <set>
#include <vector>

namespace term_ui {

	/**
	 * @brief The class responsible for displaying a code section.
	 *
	 * The code section begins with a file location. Then, all code lines
	 * are displayed along with highlights and highlight messages.
	 *
	 * A new line begins only after all highlight information from
	 * the previous one has been displayed, unless that highlight message
	 * is referenced in a further line (a highlight message is displayed
	 * only after all lines which reference it).
	 */
	class CodeSection {
	public:
		/**
		 * @brief The class responsible for displaying a file location
		 * in a standardized format: `filename:line:column`.
		 *
		 */
		class Location {
			std::string file;
			u32         line, col;

		public:
			// Construct the location from the corresponding data provided
			// by the view manager.
			Location(const view::CodeMetadata& metadata);

			/**
			 * @brief Print the file location to the output stream.
			 *
			 * @param tab_space The column in which the location should begin.
			 * @param out The output stream.
			 */
			void print(u32 tab_space, std::ostream& out) const;

			// Construct a new location without a gRPC object. For testing only.
			Location(const std::string& file, u32 line, u32 col);
		};

	private:
		// The code location.
		Location location;
		// The code lines, in a specific order.
		std::vector<CodeLine> lines;
		// The mapping of highlight message contents. Parts of a line
		// may reference the integer ID of these messages.
		base::HashMap<u32, PointerMessage> pointers;
		/**
		 * @brief The mapping of last references to highlight messages.
		 *
		 * Each highlight (a *group*) is assigned a pair (line number,
		 * column number) which represent the last moment that highlight
		 * is referenced in the code section.
		 *
		 * Note that the line number refers to the index in the `lines`
		 * vector and NOT the actual displayed line (those may differ).
		 */
		base::HashMap<u32, std::pair<u32, u32>> last_of_group;
		/**
		 * @brief The column in which the code lines should begin.
		 *
		 * All code lines are preceeded by an optional line number
		 * and a vertical bar `|`. These should fit before the `tab_space`
		 * column.
		 */
		u32 tab_space;

	public:
		// Construct the code section from the correseponding data provided
		// by the view manager.
		CodeSection(const view::CodeSection& section);

		/**
		 * @brief Print the code section to the output stream.
		 *
		 * @param out The output stream.
		 */
		void print(std::ostream& out) const;

		// Construct a new code section without a gRPC object. For testing only.
		CodeSection(
			Location                           location,
			std::vector<CodeLine>              lines,
			base::HashMap<u32, PointerMessage> pointers
		);

	private:
		// Perform preprocessing. Compute the `last_of_group` field
		// and the `tab_space` field, so that all line numbers fit before
		// the vertical bar.
		void computeLastOfAndTabSpace();

		/**
		 * @brief The result of a highlighting operation.
		 *
		 * The highlighting operation may have various outcomes, depending
		 * on a number of factors (e.g. if the highlight message fits
		 * in the line or if an additional lowering character is required).
		 */
		enum class HighlightResult {
			// The entire highlight must be lowered.
			LowerHighlight,
			// The highlight could be displayed but the message must
			// be lowered (and no lowering character has yet been displayed).
			LowerMessageMedium,
			// Both the highlight and at least one lowering char have already
			// been displayed but the message still needs to be lowered.
			LowerMessageLast,
			// The highlighting operation for this highlight has been
			// completed successfully.
			Success
		};

		/**
		 * @brief Try to fit the message into the line or, if it does not fit,
		 * lower the message.
		 *
		 * @param str The line where the message should be placed.
		 * @param beg The column where the message should begin.
		 * @param msg The message itself.
		 * @return HighlightResult
		 */
		HighlightResult fitLoweredMessage(Line& str, u32 beg, const PointerMessage& msg) const;

		/**
		 * @brief Try to fit the highlighting into the line or, if it does not
		 * fit, lower the message or the highlight.
		 *
		 * @param str The line where the highlight should be placed.
		 * @param highlight The highlight data.
		 * @param line_no The highlighted code line index (needed
		 * for determining whether the highlight fragment is the last one).
		 * @return HighlightResult
		 */
		HighlightResult underline(Line& str, Highlight highlight, u32 line_no) const;
	};

}
