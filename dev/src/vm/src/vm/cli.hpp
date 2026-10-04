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
