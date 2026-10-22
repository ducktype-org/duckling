// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once
#include "diagnostic_arguments_forward.hpp"
#include "diagnostic_state.hpp"

namespace dia {

	/**
	 * @brief Evaluate one message tree into state representation.
	 * Underneath it loads the template from the registry and evaluates it.
	 */
	state::Diagnostic evaluateDiagnostic(const dia_args::Diagnostic& thread);
}
