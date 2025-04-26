#include "interactive_logger.hpp"

void dia::InteractiveLogger::m_log(base::Box<InteractiveMessage> message) {
	json message_json = message;
	if (dump_static) {
		// TODO: Call view manager.
		// This is just temporary:
		json j{ message };
		printer::StreamPrinter::print(j.dump(2));
		printer::StreamPrinter::newline(1);
		return;
	}
	messages.emplace_back(std::move(message));
}
