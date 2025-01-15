#include <diagnostic/logger.hpp>

// Extend Error, Warning, or Info.
class MessageRelevantToThisSituation: public Error {
	// ...

protected:
	dia::Message::Domain getDomain() const override { /* ... */ }

	std::string toStringBrief() const override {
		// ...
	}
};

// ...
int main() {
	dia::Logger logger;

	// ...

	dia::SourcePosition currentPosition = {... };
	if (<check - if - error - in - current - position>)
		logger.log(base::make_unique<MessageRelevantToThisSituation>(/* ... */));

	// ...

	if (logger.bad()) {
		bool detailed = false;
		logger.dumpLog(detailed);  // prints errors with file, position, part of code, etc.
		exit(1);
	}
}
