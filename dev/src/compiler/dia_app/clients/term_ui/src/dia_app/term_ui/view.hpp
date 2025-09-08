#pragma once
#include "diagnostic.hpp"

/**
 * @brief The number of columns used to display an info group (diagnostic)
 * separator (consisting of symbols `-`).
 *
 */
#define TERM_UI_SEPARATOR_WIDTH 80

namespace term_ui {
	/**
	 * @brief The main term-ui class responsible for displaying the entire
	 * view (all diagnostics provided by the view manager).
	 *
	 */
	class View {
		// The list of all provided diagnostics to be displayed.
		std::vector<Diagnostic> diags;

	public:
		/**
		 * @brief Construct the view from the data provided by the view manager.
		 *
		 * @param view The provided data. The type comes from the compiled
		 * gRPC protocol.
		 */
		View(const view::ViewResponse& view);

		/**
		 * @brief Print this view to output stream.
		 *
		 * @param out The output stream.
		 * @param use_color Whether to use coloring in the displayed view.
		 */
		void print(std::ostream& out, bool use_color) const;
	};
}
