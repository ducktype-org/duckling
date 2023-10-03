#include "error_state.hpp"

namespace tpc {
	// @TODO: change to sth working with utf8
	void ErrorState::logError(const lexer::SourcePosition& position, std::string_view message) {
		err_count++;
		errorLog.add({
			{{ "error:", printer::Color::BRIGHT_RED },
             // @TODO: add file location info
		      position.genErrorMsg(message)},
			printer::MessageType::ERROR,
			0  // @TODO: maybe change level
		});
	}

	void ErrorState::failAndLog(const lexer::SourcePosition& position, std::string_view message) {
		setFail();
		logError(position, message);
	}

	void ErrorState::dumpLog(std::ostream& stream) const { errorLog.print(stream); }

	usize ErrorState::errCount() const { return err_count; }
}
