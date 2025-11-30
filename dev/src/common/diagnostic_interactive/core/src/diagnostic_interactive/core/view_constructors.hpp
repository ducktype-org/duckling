#include "diagnostic_state.hpp"
#include "term_ui_view.hpp"

namespace dia_int {

	std::string constructTextView(CRef<state::Component> component);

	namespace term_ui_view {
		Diagnostic constructTreeView(const state::Diagnostic& state);
	}
}
