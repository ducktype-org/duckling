#pragma once


#include <base/pointers/box.hpp>

#include <iostream>
#include <vector>

namespace dia_int {
	class MessageBase;
}

DEFAULT_BOX_PTR_DELETER_DECLARATION(dia_int::MessageBase);

namespace dia_int {
	class Logger {
		std::vector<Box<MessageBase>> diagnostics;

	public:
		Logger();

		void log(Box<MessageBase> message);

		/**
		 * @brief Check if any error messages have been logged.
		 */
		[[nodiscard]] bool bad() const;

		void dumpLog(std::ostream& out = std::cout);

		[[nodiscard]] u64 messageCount() const;
	};
}
