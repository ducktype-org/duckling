#pragma once

#include <mutex>

namespace vm::debugger::cli::impl {
	struct ImplementationSpecific {
		std::mutex output_mutex;
		bool       running;
	};
}

// override so linter knows whats going on
#ifdef USE_REPLXX
	#undef USE_REPLXX
#endif

#include <vm/debugger/UI/CLI/cli.hpp>
