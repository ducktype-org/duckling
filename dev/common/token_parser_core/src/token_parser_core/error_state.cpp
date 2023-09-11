#include "error_state.hpp"

namespace tpc {
	// @TODO: change to sth working with utf8
	void ErrorState::logError(const lexer::SourcePosition& position, const std::string& message) {
		err_count++;
		errorLog.add({
			{
				{"error:", printer::Color::BRIGHT_RED},
				// @TODO:  add file location info
				{position.getSourceChars() + ": "},
				{message + "\n"}
			},
			printer::MessageType::ERROR, 0 // @TODO: maybe change level
		});
	}

	void ErrorState::failAndLog(const lexer::SourcePosition& position, const std::string& message) {
		setFail(); logError(position, message);
	}

	void ErrorState::dumpLog(std::ostream& stream) const {
		errorLog.print(stream);
	}

	usize ErrorState::errCount() const {
		return err_count;
	}
}
