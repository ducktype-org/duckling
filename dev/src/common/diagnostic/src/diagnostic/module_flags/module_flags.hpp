// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <base/pointers/ref.hpp>

#include <ostream>

namespace dia {
	/**
	 * @brief If set, the logger will immediately print messages to the given stream
	 * as they are logged. If left empty, no immediate printing will occur.
	 */
	extern constinit MRef<std::ostream> immediate_print_stream;

	/**
	 * @brief Just a setter for the immediate print stream variable.
	 * @param stream The stream to which the messages should be immediately printed.
	 */
	void configureImmediatePrint(MRef<std::ostream> stream);

	void configureTerminalPrinterColors(bool use_colors);
}
