#include "frontend.hpp"

#include <cctype>
#include <cstdlib>
#include <string>

namespace compiler::repl {
	ReplFrontend::ReplFrontend(bool completions_enabled, bool bracketed_paste_enabled):
		  m_impl(completions_enabled, bracketed_paste_enabled) {}

	void ReplFrontend::printWelcome() const { m_impl.printWelcome(); }

	std::string ReplFrontend::readLine(bool decorative_output_enabled) {
		return m_impl.readLine(decorative_output_enabled);
	}

	void ReplFrontend::printHistory() const { m_impl.printHistory(); }

	void ReplFrontend::addHistoryEntry(std::string_view entry) { m_impl.addHistoryEntry(entry); }

	void ReplFrontend::clearScreen() { m_impl.clearScreen(); }

	void ReplFrontend::clearHistory() { m_impl.clearHistory(); }

	void ReplFrontend::printHelp() const { m_impl.printHelp(); }

	void ReplFrontend::printCompletions(std::string_view prefix) const {
		m_impl.printCompletions(prefix);
	}

}  // namespace compiler::repl
