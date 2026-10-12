// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once


#include <base/pointers/box.hpp>

#include <diagnostic/core/common_classes.hpp>
#include <diagnostic/core/diagnostic_arguments_forward.hpp>
#include <diagnostic/message_fwd.hpp>
#include <diagnostic/module_flags/module_flags.hpp>  // IWYU pragma: export

#include <iostream>
#include <ostream>
#include <vector>

namespace dia {
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

		[[nodiscard]] bool good() const;

		[[nodiscard]] bool bad() const;

		void terminalPrint(std::ostream& out = std::cerr) const;

		void dumpLog(bool, std::ostream& out) const { terminalPrint(out); }

		/**
		 * @brief Collect all logged diagnostics into the provided output vector.
		 * @param[out] out_messages Vector to collect diagnostics into.
		 */
		void collectDiagnostics(std::vector<CRef<dia_args::Diagnostic>>& out_messages) const;

		/**
		 * @brief Collect all logged diagnostics into the provided output vector
		 * with the positions updated using the provided function and node PST hashes.
		 * @param[out] out_messages Vector to collect diagnostics into.
		 */
		void collectAndUpdatePositionDiagnostics(
			std::vector<CRef<dia_args::Diagnostic>>& out_messages,
			const UpdatePositionFunc&                update_func
		);

		/**
		 * @brief Use it for testing purposes only,
		 * it can invalidate the references to stored messages
		 * borrowed by the collectDiagnostics method.
		 */
		void clear();

		[[nodiscard]] u64 messageCount() const;

		[[nodiscard]] u64 errorCount() const;

		/** @brief Number of logged diagnostics with type "warning". */
		[[nodiscard]] u64 warningCount() const;

		/**
		 * @brief Evaluate diagnostic to terminal message and print it to the given stream.
		 * It is used internally by the Logger to print immediate messages, but also can be used
		 * externally.
		 */
		static void evaluateToTerminalMessage(
			CRef<dia_args::Diagnostic> diagnostic_args,
			std::ostream&              out,
			bool                       catch_exceptions = true
		);

		/**
		 * @brief Log diagnostics from another Logger into this one.
		 * The other Logger is emptied.
		 */
		void logFromLogger(Logger& other);
	};

	/**
	 * @brief Helper function to create a message and evaluate it to an output stream using terminal
	 * printer.
	 */
	template<typename MsgClass, typename... Args>
	void testDiagnosticMessage(std::ostream& out, Args&&... args) {
		static_assert(
			std::is_base_of_v<MessageBase, MsgClass>, "MsgClass must derive from MessageBase"
		);

		// Create the message
		auto obj = base::makeBox<MsgClass>(std::forward<Args>(args)...)->buildDiagnosticFile();

		// Evaluate to terminal (or any other stream)
		Logger::evaluateToTerminalMessage(obj.ref(), out, false);
	}
}
