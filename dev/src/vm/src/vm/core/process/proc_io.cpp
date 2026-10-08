// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "proc_io.hpp"

#include <base/except/exceptions.hpp>

vm::ProcIORedirecter vm::ProcIO::attach(std::istream& input_source, std::ostream& output_dst) {
	auto lck = lock();
	CORE_ASSERT(!attached, "This is not possible");
	return { *this, input_source, output_dst };
}
