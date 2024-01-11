#include "error_state.hpp"

#include <utility>

namespace dia {
	void ErrorState::logError(const SourcePosition& position, std::string_view message) {
		err_count++;
		errorLog.add(position.genErrorMsg(message));
	}

	void ErrorState::failAndLog(const SourcePosition& position, std::string_view message) {
		setFail();
		logError(position, message);
	}

	void ErrorState::logError(const printer::Message& message) {
		err_count++;
		errorLog.add(message);
	}

	void ErrorState::failAndLog(printer::Message message) {
		setFail();
		logError(std::move(message));
	}

	void ErrorState::dumpLog(std::ostream& stream) const { errorLog.print(stream); }

	usize ErrorState::errCount() const { return err_count; }
}
