#include "logger.hpp"

#include "message.hpp"

#include <diagnostic_interactive/core/diagnostic_arguments.hpp>
#include <diagnostic_interactive/core/template_evaluation.hpp>
#include <diagnostic_interactive/core/template_registry.hpp>
#include <diagnostic_interactive/core/view_constructors.hpp>
#include <diagnostic_interactive/term_ui/printers.hpp>

namespace dia_int {

	std::ostream* Logger::immediate_print_stream = nullptr;
	bool          Logger::immediate_print        = false;

	void Logger::terminalPrint(std::ostream& out) {
		for (auto& diagnostic: diagnostics) evaluateToTerminalMessage(diagnostic.refMut(), out);
	}

	void Logger::log(Box<MessageBase> message) {
		if (message->isError()) has_error = true;

		diagnostics.emplace_back(message->buildDiagnosticFile());
		if (immediate_print)
			evaluateToTerminalMessage(diagnostics.back().refMut(), *immediate_print_stream);
	}

	Logger::Logger(): has_error(false) {
		dia_int::TemplateRegistrySingleton::setInstance(
			makeBox<dia_int::TemplateResistryMainProvider>()
		);
	}

	usize Logger::messageCount() const { return diagnostics.size(); }

	bool Logger::hasError() const { return has_error; }

	void Logger::clear() {
		diagnostics.clear();
		has_error = false;
	}

	void Logger::evaluateToTerminalMessage(
		CRef<dia_args::Diagnostic> diagnostic_args, std::ostream& out
	) {
		try {
			// std::cout << diagnostic_args->toJson().dump(4) << "\n\n";
			auto state = dia_int::evaluateDiagnostic(*diagnostic_args);
			// state.debugPrint(std::cout);
			auto view = dia_int::constructTreeView(state);
			term_ui::print(view, out);
		} catch (const std::exception& e) {
			out << "Error while printing diagnostic message: \n" << e.what() << "\n";
		}
	}

	void Logger::evaluateToLanguageServerMessage(CRef<dia_args::Diagnostic>, std::ostream&) {}

}
