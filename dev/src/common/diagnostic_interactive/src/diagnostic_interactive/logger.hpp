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

		void dumpLog(std::ostream& out = std::cout);

		[[nodiscard]] usize messageCount() const;
	};
}
