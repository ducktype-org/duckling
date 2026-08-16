#include "diagnostic_state.hpp"
#include "term_ui_view.hpp"

namespace dia {

	/**
	 * @brief Construct a plain-text view from diagnostic state.
	 */
	std::string constructTextView(CRef<state::Component> component);

	/**
	 * @brief Construct a tree term_ui_view from diagnostic state.
	 */
	term_ui_view::Diagnostic constructTreeView(const state::Diagnostic& state);

}
