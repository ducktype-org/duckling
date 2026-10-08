// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "module_flags.hpp"

#include <rang.hpp>

#include <sstream>

namespace term_ui {
	bool use_color = true;

	void configureColoring(bool use) {
		use_color = use;
		// Always force rang so it emits escape codes even when printing
		// to stringstreams (which are not recognized as TTYs).
		rang::setControlMode(rang::control::Force);
	}
}
