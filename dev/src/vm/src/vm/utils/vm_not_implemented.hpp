#pragma once

#include <base/except/exceptions.hpp>

#include <string_view>

namespace vm {
	/**
	 * Exception thrown when a VM operation is not implemented.
	 */
	class VMNotImplemented final: public base::Exception {
		std::string message;

	public:
		VMNotImplemented(): message("VM Operation not implemented") {}

		VMNotImplemented(std::string_view msg): message(msg) {}

		[[nodiscard]]
		constexpr const char* what() const noexcept override {
			return message.c_str();
		}
	};
}
