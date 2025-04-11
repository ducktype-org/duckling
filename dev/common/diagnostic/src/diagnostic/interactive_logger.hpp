#include <base/box.hpp>
#include <string_view>
#include "interactive_message.hpp"

namespace dia {
	class InteractiveLogger {
	private:
		std::vector<Box<InteractiveMessage>> messages{};

	public:
		bool dump_static = true;

		void log(base::Box<InteractiveMessage> message) {
			if (dump_static) {
				// TODO: Call view manager.
				// This is just temporary:
				json j{ message };
				printer::StreamPrinter::print(j.dump(2));
				printer::StreamPrinter::newline(1);
				return;
			}
			messages.push_back(std::move(message));
		}

		void dump(std::string_view filename) {
			// TODO:
		}
	};
}
