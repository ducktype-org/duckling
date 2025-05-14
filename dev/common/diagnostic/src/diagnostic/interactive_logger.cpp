#include "interactive_logger.hpp"
#include <dia_app/view_manager/view_manager.hpp>

void dia::InteractiveLogger::m_log(base::Box<InteractiveMessage> message) {
	json message_json = message;
	if (dump_static) {
		auto view_manager = dia_app::view_manager::ViewManager::createFromJson(message_json);
		::view::ViewResponse vm_data;
		view_manager.getView(&vm_data);
		// Now we have view data in the format declared in view.proto
		// This is just temporary:
		json j{ message };
		printer::StreamPrinter::print(j.dump(2), std::cout);
		printer::StreamPrinter::newline(1, std::cout);
		return;
	}
	messages.emplace_back(std::move(message));
}
