// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "logger.hpp"

#include "message.hpp"

#include <base/collections/optional.hpp>

#include <diagnostic/core/diagnostic_arguments.hpp>
#include <diagnostic/core/template_evaluation.hpp>
#include <diagnostic/core/template_registry.hpp>
#include <diagnostic/core/view_constructors.hpp>
#include <diagnostic/term_ui/printers.hpp>

#include <sstream>

namespace dia {

	void Logger::terminalPrint(std::ostream& out) const {
		for (auto& diagnostic: diagnostics) evaluateToTerminalMessage(diagnostic.ref(), out);
	}

	void Logger::log(Box<MessageBase> message) {
		if (message->isError()) has_error = true;

		diagnostics.emplace_back(message->buildDiagnosticFile());

		if_opt_some(immediate_print_stream, stream) {
			std::stringstream ss;  // For multithreading safety.
			evaluateToTerminalMessage(diagnostics.back().ref(), ss);
			stream << ss.str();
		}
	}

	Logger::Logger(): has_error(false) {
		dia::TemplateRegistrySingleton::setInstance(makeBox<dia::TemplateResistryMainProvider>());
	}

	usize Logger::messageCount() const { return diagnostics.size(); }

	u64 Logger::errorCount() const {
		u64 count = 0;
		for (const auto& diag: diagnostics)
			if (diag->main_message.metadata.type == "error") count++;
		return count;
	}

	u64 Logger::warningCount() const {
		u64 count = 0;
		for (const auto& diag: diagnostics)
			if (diag->main_message.metadata.type == "warning") count++;
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
			auto state = dia::evaluateDiagnostic(*diagnostic_args);
			// state.debugPrint(std::cout);
			auto view = dia::constructTreeView(state);
			term_ui::print(view, out);
		} catch (const std::exception& e) {
			out << "Error while printing diagnostic message: \n" << e.what() << "\n";
			if (not catch_exceptions) throw;
		}
	}

	void Logger::collectDiagnostics(std::vector<CRef<dia_args::Diagnostic>>& out_messages) const {
		for (const auto& msg: diagnostics) out_messages.emplace_back(msg.ref());
	}

	void Logger::collectAndUpdatePositionDiagnostics(
		std::vector<CRef<dia_args::Diagnostic>>& out_messages, const UpdatePositionFunc& update_func
	) {
		for (const auto& msg: diagnostics) {
			msg->updateCodeLocationComponents(update_func);
			out_messages.emplace_back(msg.ref());
		}
	}

	bool Logger::bad() const { return has_error; }

	void Logger::logFromLogger(Logger& other) {
		std::ranges::move(other.diagnostics, std::back_inserter(diagnostics));
		has_error |= other.has_error;

		other.diagnostics.clear();
		other.has_error = false;
	}

}
