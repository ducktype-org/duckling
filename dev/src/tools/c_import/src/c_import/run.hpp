#pragma once

#include "options.hpp"

namespace c_import {

	/** Reads the headers, renders the package and writes it. Returns a process exit code. */
	int run(const Options& options);

}
