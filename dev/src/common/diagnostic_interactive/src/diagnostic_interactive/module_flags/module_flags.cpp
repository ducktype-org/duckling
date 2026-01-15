#include "module_flags.hpp"

namespace dia_int {
	constinit MRef<std::ostream> immediate_print_stream = nullptr;

	void configureImmediatePrint(MRef<std::ostream> stream) { immediate_print_stream = stream; }
}
