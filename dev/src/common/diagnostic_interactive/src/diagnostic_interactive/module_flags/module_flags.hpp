#pragma once

#include <ostream>
#include <base/pointers/ref.hpp>

namespace dia_int {
	/**
	 * @brief If set, the logger will immediately print messages to the given stream
	 * as they are logged. If left empty, no immediate printing will occur.
	 */
	extern constinit MRef<std::ostream> immediate_print_stream;

	void configureImmediatePrint(MRef<std::ostream> stream);
}
