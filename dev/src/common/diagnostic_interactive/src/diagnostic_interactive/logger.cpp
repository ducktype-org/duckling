#include "logger.hpp"

#include "message.hpp"

#include <diagnostic_interactive/core/template_evaluation.hpp>
#include <diagnostic_interactive/core/template_registry.hpp>
#include <diagnostic_interactive/core/view_constructors.hpp>
#include <diagnostic_interactive/core/diagnostic_arguments.hpp>
#include <diagnostic_interactive/term_ui/printers.hpp>

namespace dia_int {

	void Logger::dumpLog(std::ostream& out) {
		for (auto& msg: diagnostics) {
			try {
				auto diagnostic_args = msg->buildDiagnosticFile();
				std::cout << diagnostic_args->toJson().dump(4) << "\n\n";
				auto state = dia_int::evaluateDiagnostic(*diagnostic_args);
				state.debugPrint(std::cout);
				auto view = dia_int::constructTreeView(state);
				term_ui::print(view, out);
			} catch (const std::exception& e) {
				out << "Error while printing diagnostic message: \n" << e.what() << "\n";
			}
		}
		diagnostics.clear();
	}

	void Logger::log(Box<MessageBase> message) { diagnostics.push_back(std::move(message)); }

	Logger::Logger() {
		dia_int::TemplateRegistrySingleton::setInstance(makeBox<dia_int::TemplateResistryMainProvider>());
	}
}
