#pragma once
#include "info.hpp"

namespace term_ui {
	/**
	 * @brief The class responsible for displaying one info group
	 * (or a *diagnostic* in the gRPC protocol naming convention).
	 *
	 */
	class Diagnostic {
		// The list of infos to be displayed in this diagnostic, one after
		// another.
		std::vector<Info> infos;

	public:
		// Construct the diagnostic from the corresponding data provided by
		// the view manager.
		Diagnostic(const view::Diagnostic& diag);

		/**
		 * @brief Print this diagnostic to output stream.
		 *
		 * @param out The output stream.
		 */
		void print(std::ostream& out) const;
	};
}
