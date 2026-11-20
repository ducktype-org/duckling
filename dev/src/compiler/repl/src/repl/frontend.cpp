// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "frontend.hpp"

#include <cctype>
#include <cstdlib>
#include <string>

namespace compiler::repl {
	ReplFrontend::ReplFrontend(
		bool completions_enabled, bool bracketed_paste_enabled, bool decorative_output
	):
		  m_impl(completions_enabled, bracketed_paste_enabled, decorative_output) {}

	void ReplFrontend::printWelcome() const { m_impl.printWelcome(); }

	std::string ReplFrontend::readLine() { return m_impl.readLine(); }

	void ReplFrontend::printHistory() const { m_impl.printHistory(); }

	void ReplFrontend::addHistoryEntry(std::string_view entry) { m_impl.addHistoryEntry(entry); }

	void ReplFrontend::clearScreen() { m_impl.clearScreen(); }

	void ReplFrontend::clearHistory() { m_impl.clearHistory(); }

	void ReplFrontend::printHelp() const { m_impl.printHelp(); }

	void ReplFrontend::printCompletions(std::string_view prefix) const {
		m_impl.printCompletions(prefix);
	}

}  // namespace compiler::repl
