// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include <init/init.hpp>

int main() {
	init::InitObject _;
	return 0;
	/**
	 * Linter fails on exit(0) because it's not thread-safe.
	 */
}
