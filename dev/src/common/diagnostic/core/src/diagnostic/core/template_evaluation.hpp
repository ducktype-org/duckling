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
