// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

/**
 * @file cli.hpp
 * @brief Command line interface for the VM.
 */

#pragma once

#include <vm/core/supervisor/supervisor.hpp>

int cli(
	const std::vector<fs::File>&    files,
	const std::vector<std::string>& args     = {},
	const vm::api::ProcessConfig&   options  = {},
	const std::vector<std::string>& ffi_libs = {}
);
