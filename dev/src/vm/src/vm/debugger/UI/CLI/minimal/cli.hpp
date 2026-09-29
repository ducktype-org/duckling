#pragma once

#include <mutex>

namespace vm::debugger::cli::impl {
	struct ImplementationSpecific {
		std::mutex output_mutex;
		bool       running;
	};
}

#include <vm/debugger/UI/CLI/cli.hpp>
