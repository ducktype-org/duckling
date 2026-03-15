#include "module_flags.hpp"

#include <diagnostic_interactive/term_ui/module_flags/module_flags.hpp>

namespace dia_int {
	constinit MRef<std::ostream> immediate_print_stream = nullptr;

	void configureImmediatePrint(MRef<std::ostream> stream) { immediate_print_stream = stream; }

	void configureTerminalPrinterColors(bool use_colors) { term_ui::configureColoring(use_colors); }
}
