#pragma once

#include <stdexcept>

namespace vm {
	/**
	 * Exception thrown when a VM operation is not implemented.
	 */
	class VMNotImplemented final: public std::runtime_error {
	public:
		VMNotImplemented(): std::runtime_error("VM Operation not implemented") {}

		VMNotImplemented(std::string_view msg): std::runtime_error(msg.data()) {}
	};
}
