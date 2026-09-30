#pragma once

#include <mutex>

namespace vm::debugger::cli::impl {
	struct ImplementationSpecific {
		std::mutex output_mutex;
		bool       running;
	};
}

// override so linter knows whats going on
#undef USE_REPLXX
#include <vm/debugger/UI/CLI/cli.hpp>
