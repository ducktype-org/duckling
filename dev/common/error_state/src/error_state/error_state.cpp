#include "error_state.hpp"

// @TODO: change to sth working with utf8
void ErrorState::logError(const SourcePosition& position, std::string_view message) {
	err_count++;
	errorLog.add(position.genErrorMsg(message));
}

void ErrorState::failAndLog(const SourcePosition& position, std::string_view message) {
	setFail();
	logError(position, message);
}

void ErrorState::dumpLog(std::ostream& stream) const { errorLog.print(stream); }

usize ErrorState::errCount() const { return err_count; }
