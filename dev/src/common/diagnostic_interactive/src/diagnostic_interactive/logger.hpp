#pragma once

#include "diagnostic.hpp"

#include <base/box.hpp>
#include <vector>
#include <iostream>

#include <diagnostic_interactive/term_ui/printers.hpp>
#include <diagnostic_interactive/core/template_evaluation.hpp>
#include <diagnostic_interactive/core/view_constructors.hpp>

namespace dia_int {
	class Logger {
		std::vector<Box<DiagnosticBase>> messages;

	public:
		void log(Box<DiagnosticBase> message) {
			messages.push_back(std::move(message));
		}

		void dumpLog(std::ostream& out = std::cout) {
			for (auto& msg: messages) {
				auto thread = msg->serialize();
				auto state  = dia_app::evaluateDiagnostic(thread);
				auto view   = dia_app::term_ui_view::constructTreeView(state);
				term_ui::print(view, out);
			}
			messages.clear();
		}
	};
}
