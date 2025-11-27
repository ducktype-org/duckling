#pragma once

#include "diagnostic.hpp"
#include "diagnostic_interactive/core/template_registry.hpp"

#include <diagnostic_interactive/core/template_evaluation.hpp>
#include <diagnostic_interactive/core/view_constructors.hpp>
#include <diagnostic_interactive/term_ui/printers.hpp>

#include <base/pointers/box.hpp>

#include <iostream>
#include <vector>

namespace dia_int {
	class Logger {
		std::vector<Box<DiagnosticBase>> diagnostics;

	public:
		Logger() {
			TemplateRegistry::setInstance(makeBox<dia_app::TemplateResistryEmbeddedProvider>());
		}

		void log(Box<DiagnosticBase> message);

		void dumpLog(std::ostream& out = std::cout);
	};
}
