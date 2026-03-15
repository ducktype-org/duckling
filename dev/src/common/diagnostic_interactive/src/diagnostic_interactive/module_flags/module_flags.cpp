#include "module_flags.hpp"
#include <diagnostic_interactive/term_ui/module_flags/module_flags.hpp>
namespace dia_int {
	constinit MRef<std::ostream> immediate_print_stream = nullptr;

	void configureImmediatePrint(MRef<std::ostream> stream, bool use_colors) {
		immediate_print_stream = stream;
		term_ui::configureColoring(use_colors);
	}
}
