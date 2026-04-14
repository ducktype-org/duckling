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
			std::stringstream ss;  // For multithreading safety.
			evaluateToTerminalMessage(diagnostics.back().refMut(), ss);
			stream << ss.str();
		}
	}

	Logger::Logger(): has_error(false) {
		dia_int::TemplateRegistrySingleton::setInstance(
			makeBox<dia_int::TemplateResistryMainProvider>()
		);
	}

	usize Logger::messageCount() const { return diagnostics.size(); }

	u64 Logger::errorCount() const {
		u64 count = 0;
		for (const auto& diag: diagnostics)
			if (diag->main_message.metadata.type == "error") count++;
		return count;
	}

	bool Logger::hasErrors() const { return has_error; }

	bool Logger::good() const { return not hasErrors(); }

	void Logger::clear() {
		diagnostics.clear();
		has_error = false;
	}

	void Logger::evaluateToTerminalMessage(
		CRef<dia_args::Diagnostic> diagnostic_args, std::ostream& out, bool catch_exceptions
	) {
		try {
			// std::cout << diagnostic_args->toJson().dump(4) << "\n\n";
			auto state = dia_int::evaluateDiagnostic(*diagnostic_args);
			// state.debugPrint(std::cout);
			auto view = dia_int::constructTreeView(state);
			term_ui::print(view, out);
		} catch (const std::exception& e) {
			out << "Error while printing diagnostic message: \n" << e.what() << "\n";
			if (not catch_exceptions) throw;
		}
	}

	void Logger::collectDiagnostics(std::vector<CRef<dia_args::Diagnostic>>& out_messages) const {
		for (const auto& msg: diagnostics) out_messages.emplace_back(msg.ref());
	}

	void Logger::collectPositionUpdatedDiagnostics(
		std::vector<CRef<dia_args::Diagnostic>>& out_messages, const UpdatePositionFunc& update_func
	) {
		for (const auto& msg: diagnostics) {
			msg->updateCodeLocationComponents(update_func);
			out_messages.emplace_back(msg.ref());
		}
	}

	bool Logger::bad() const { return has_error; }

	void Logger::mergeWith(Logger&& other) {
		std::ranges::move(other.diagnostics, std::back_inserter(diagnostics));
		has_error = has_error || other.has_error;
		auto _    = std::move(other);
	}

}
