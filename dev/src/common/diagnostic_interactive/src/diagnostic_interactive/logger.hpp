#pragma once


#include <diagnostic_interactive/core/diagnostic_arguments_forward.hpp>
#include <diagnostic_interactive/module_flags/module_flags.hpp>  // IWYU pragma: export

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

	public:
		Logger();

		void log(Box<MessageBase> message);

		/**
		 * @brief Check if any error messages have been logged.
		 */
		[[nodiscard]] bool hasErrors() const;

		void terminalPrint(std::ostream& out = std::cerr);

		/**
		 * @brief Collect all logged diagnostics into the provided output vector.
		 * @param[out] out_messages Vector to collect diagnostics into.
		 */
		void collectDiagnostics(std::vector<CRef<dia_args::Diagnostic>>& out_messages);

		/**
		 * @brief Use it for testing purposes only,
		 * it can invalidate the references to stored messages
		 * borrowed by the collectDiagnostics method.
		 */
		void clear();

		[[nodiscard]] u64 messageCount() const;

		/**
		 * @brief Evaluate diagnostic to terminal message and print it to the given stream.
		 * It is used internally by the Logger to print immediate messages, but also can be used
		 * externally.
		 */
		static void evaluateToTerminalMessage(
			CRef<dia_args::Diagnostic> diagnostic_args, std::ostream& out
		);

		// Placeholder for future implementation
		// static void evaluateToLanguageServerMessage(
		// 	CRef<dia_args::Diagnostic> diagnostic_args, std::ostream& out
		// );
	};
}
