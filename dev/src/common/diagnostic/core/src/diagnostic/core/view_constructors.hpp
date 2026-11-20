// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

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
