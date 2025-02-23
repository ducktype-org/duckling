#include <diagnostic/logger.hpp>

// Extend Error, Warning, or Info.
class MessageRelevantToThisSituation: public dia::Error {
	// ...

protected:
	[[nodiscard]]
	dia::Message::Domain getDomain() const override {
		// choose relevant domain:
		return dia::Message::Domain::Misc;
	}

	[[nodiscard]]
	std::string toStringBrief() const override {
		// provide brief description of the error:
		return "some example message";
	}
};

// ...
int main() {
	dia::Logger logger;

	// ...

	// choose correct position:
	dia::SourcePosition current_position = dia::SourcePosition::fakePosition();

	// log message:
	// some more info will often be passed to the constructor here.
	logger.log(makeBox<MessageRelevantToThisSituation>(current_position));

	// ...

	if (logger.bad()) {
		bool detailed = false;
		logger.dumpLog(detailed);  // prints errors with file, position, part of code, etc.
		exit(1);
	}
}
