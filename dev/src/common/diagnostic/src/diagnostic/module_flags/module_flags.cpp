// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "module_flags.hpp"

#include <diagnostic/term_ui/module_flags/module_flags.hpp>

namespace dia {
	constinit MRef<std::ostream> immediate_print_stream = nullptr;

	void configureImmediatePrint(MRef<std::ostream> stream) { immediate_print_stream = stream; }

	void configureTerminalPrinterColors(bool use_colors) { term_ui::configureColoring(use_colors); }
}
