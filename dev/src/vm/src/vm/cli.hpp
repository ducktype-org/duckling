/**
 * @file cli.hpp
 * @brief Command line interface for the VM.
 */

#pragma once

#include <vm/api/settings.hpp>
#include <vm/core/supervisor/supervisor.hpp>

int cli(
	const std::vector<fs::File>&    files,
	const std::vector<std::string>& args     = {},
	const vm::api::ProcessSettings& settings = {},
	const std::vector<std::string>& ffi_libs = {}
);
int cli();
