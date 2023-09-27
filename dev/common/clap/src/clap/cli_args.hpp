#pragma once

#include "base/ints.hpp"

namespace clap {
	struct CLIArgs {
		const i32                argc;
		const char *const *const argv;
	};
}
