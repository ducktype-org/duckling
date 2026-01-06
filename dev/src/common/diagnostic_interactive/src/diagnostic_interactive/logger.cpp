#include "logger.hpp"

#include "message.hpp"

#include <diagnostic_interactive/core/diagnostic_arguments.hpp>
#include <diagnostic_interactive/core/template_evaluation.hpp>
#include <diagnostic_interactive/core/template_registry.hpp>
#include <diagnostic_interactive/core/view_constructors.hpp>
#include <diagnostic_interactive/term_ui/printers.hpp>

#include <base/collections/optional.hpp>

namespace dia_int {

	void Logger::terminalPrint(std::ostream& out) {
		for (auto& diagnostic: diagnostics) evaluateToTerminalMessage(diagnostic.refMut(), out);
	}

	void Logger::log(Box<MessageBase> message) {
		if (message->isError()) has_error = true;

		diagnostics.emplace_back(message->buildDiagnosticFile());

		if_opt_some(immediate_print_stream, stream) {
			evaluateToTerminalMessage(diagnostics.back().refMut(), stream);
		}
	}

	Logger::Logger(): has_error(false) {
		dia_int::TemplateRegistrySingleton::setInstance(
			makeBox<dia_int::TemplateResistryMainProvider>()
		);
	}

	usize Logger::messageCount() const { return diagnostics.size(); }

	bool Logger::hasErrors() const { return has_error; }

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

	void Logger::collectDiagnostics(std::vector<CRef<dia_args::Diagnostic>>& out_messages) {
		for (const auto& msg: diagnostics) out_messages.emplace_back(msg.refMut());
	}
}
