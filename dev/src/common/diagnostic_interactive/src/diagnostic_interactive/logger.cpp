#include "logger.hpp"

#include "message.hpp"

#include <diagnostic_interactive/core/template_evaluation.hpp>
#include <diagnostic_interactive/core/template_registry.hpp>
#include <diagnostic_interactive/core/view_constructors.hpp>
#include <diagnostic_interactive/term_ui/printers.hpp>

namespace dia_int {

	void Logger::dumpLog(std::ostream& out) {
		for (auto& msg: diagnostics) {
			try {
				auto diagnostic_file = msg->buildDiagnosticFile();
				std::cout << diagnostic_file->toJson().dump(4) << "\n\n";
				auto state = dia_int::evaluateDiagnostic(*diagnostic_file);
				state.debugPrint(std::cout);
				auto view = dia_int::term_ui_view::constructTreeView(state);
				term_ui::print(view, out);
			} catch (const std::exception& e) {
				out << "Error while printing diagnostic message: \n" << e.what() << "\n";
			}
		}
		diagnostics.clear();
	}

	void Logger::log(Box<MessageBase> message) { diagnostics.push_back(std::move(message)); }

	Logger::Logger() {
		dia_int::TemplateRegistry::setInstance(makeBox<dia_int::TemplateResistryMainProvider>());
	}
}
