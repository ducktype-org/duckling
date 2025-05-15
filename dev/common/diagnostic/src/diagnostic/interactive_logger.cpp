#include "interactive_logger.hpp"
#include <dia_app/view_manager/view_manager.hpp>
#include <dia_app/term_ui/message.hpp>

void dia::InteractiveLogger::m_log(base::Box<InteractiveMessage> message) {
	json message_json = message;

	// Create a view manager instance for static message.
	auto view_manager = dia_app::view_manager::ViewManager::createFromJson(message_json);
	::view::ViewResponse vm_data;
	view_manager.getView(&vm_data);

	// Format and print the static message to the terminal.
	term_ui::Message term_msg(vm_data);
	term_msg.print();
	
	if (dump_static) {
		// Now we have view data in the format declared in view.proto
		// This is just temporary:
		json j{ message };
		printer::StreamPrinter::print(j.dump(2), std::cout);
		printer::StreamPrinter::newline(1, std::cout);
		return;
	}
	messages.emplace_back(std::move(message));
}
