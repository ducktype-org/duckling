#include "frontend.hpp"

#include <cctype>
#include <cstdlib>
#include <string>

namespace compiler::repl {
	ReplFrontend::ReplFrontend(): m_impl() {}

	void ReplFrontend::printWelcome() const { m_impl.printWelcome(); }

	std::string ReplFrontend::readLine() { return m_impl.readLine(); }

	void ReplFrontend::printHistory() const { m_impl.printHistory(); }

	void ReplFrontend::clearHistory() { m_impl.clearHistory(); }

	void ReplFrontend::printHelp() const { m_impl.printHelp(); }

}  // namespace compiler::repl
