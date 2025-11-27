#pragma once

#include <base/except/exceptions.hpp>

#include <string>

namespace vm {
	class KillProcessException: public base::Exception {
		std::string message;

	public:
		KillProcessException(std::string message = "Process killed"): message(std::move(message)) {
			this->message += '\0';
		}

		[[nodiscard]]
		const char* what() const noexcept override {
			return message.data();
		}
	};
}
