#include "module_flags.hpp"

#include <diagnostic/term_ui/module_flags/module_flags.hpp>

namespace dia {
	constinit MRef<std::ostream> immediate_print_stream = nullptr;

	void configureImmediatePrint(MRef<std::ostream> stream) { immediate_print_stream = stream; }

	void configureTerminalPrinterColors(bool use_colors) { term_ui::configureColoring(use_colors); }
}
