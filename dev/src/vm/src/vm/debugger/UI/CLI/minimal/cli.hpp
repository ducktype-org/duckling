#pragma once

namespace vm::debugger::cli::impl {
	struct ImplementationSpecific {
		std::mutex output_mutex;
	};
}

#include <vm/debugger/UI/CLI/cli.hpp>
