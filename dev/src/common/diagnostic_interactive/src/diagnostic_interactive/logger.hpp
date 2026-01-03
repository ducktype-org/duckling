#pragma once


#include <diagnostic_interactive/core/diagnostic_arguments_forward.hpp>

#include <base/pointers/box.hpp>

#include <iostream>
#include <ostream>
#include <vector>

namespace dia_int {
	class MessageBase;
}

DEFAULT_BOX_PTR_DELETER_DECLARATION(dia_int::MessageBase);

namespace dia_int {
	class Logger {
		std::vector<Box<dia_args::Diagnostic>> diagnostics;
		bool                                   has_error;

		static std::ostream* immediate_print_stream;
		static bool          immediate_print;

	public:
		Logger();

		void log(Box<MessageBase> message);

		/**
		 * @brief Check if any error messages have been logged.
		 * @TODO: #1750 change this to okBad.
		 */
		[[nodiscard]] bool hasError() const;

		void terminalPrint(std::ostream& out = std::cerr);

		/**
		 * @brief Collect all logged diagnostics into the provided output vector.
		 * @param[out] out_messages Vector to collect diagnostics into.
		 */
		void collectDiagnostics(std::vector<CRef<dia_args::Diagnostic>>& out_messages) {
			for (const auto& msg: diagnostics) out_messages.emplace_back(msg.refMut());
		}

		/**
		 * @brief Use it for testing purposes only,
		 * it can invalidate the references to stored messages
		 * borrowed by the collectDiagnostics method.
		 */
		void clear();

		[[nodiscard]] u64 messageCount() const;

		static void evaluateToTerminalMessage(
			CRef<dia_args::Diagnostic> diagnostic_args, std::ostream& out
		);

		static void evaluateToLanguageServerMessage(
			CRef<dia_args::Diagnostic> diagnostic_args, std::ostream& out
		);

		static void configureImmediatePrint(bool enabled, std::ostream& stream = std::cerr) {
			immediate_print        = enabled;
			immediate_print_stream = &stream;
		}
	};
}
