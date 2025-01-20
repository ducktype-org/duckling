// #include <diagnostic/logger.hpp>

// // Extend Error, Warning, or Info.
// class MessageRelevantToThisSituation: public dia::Error {
// 	// ...

// protected:
// 	[[nodiscard]]
// 	dia::Message::Domain getDomain() const override { /* ... */
// 		return {};
// 	}

// 	std::string toStringBrief() const override {
// 		// ...
// 		return {};
// 	}
// };

// // ...
// int main() {
// 	dia::Logger logger;

// 	// ...

// 	dia::SourcePosition current_position = { /*...*/ };
// 	if (<check - if - error - in - current - position>)
// 		logger.log(base::make_unique<MessageRelevantToThisSituation>(/* ... */));

// 		// ...

// 		if (logger.bad()) {
// 			bool detailed = false;
// 			logger.dumpLog(detailed);  // prints errors with file, position, part of code, etc.
// 			std::exit(1);
// 		}
// }
