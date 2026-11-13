// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "helpers.hpp"

#include <vm/core/fast/fast_vmthread.hpp>
#include <vm/core/fast/runtime.hpp>
#include <vm/core/process/ivmprocess.hpp>
#include <vm/core/thread/kill_process_exception.hpp>

#include <exception>

namespace {
	vm::fast::Trap recordHostError(
		vm::fast::ThreadRuntimeState& state, const char* message
	) noexcept {
		try {
			state.trap_message = message;
		} catch (...) {
			// The trap kind alone is still reported.
		}
		return vm::fast::Trap::HostError;
	}
}

// clang-format off
#define FAST_HELPER_BOUNDARY(state, ...)                                           \
	try {                                                                          \
		__VA_ARGS__                                                                \
	} catch (const ::vm::KillProcessException&) {                                  \
		return ::vm::fast::Trap::Stopped;                                          \
	} catch (const std::exception& e) {                                            \
		return recordHostError(state, e.what());                                   \
	} catch (...) {                                                                \
		return recordHostError(state, "Unknown exception in a fast-mode helper");  \
	}
// clang-format on

vm::fast::Trap vm::fast::FastHelpers::inputI64(
	ThreadRuntimeState& state, FastVMThread& thread, i64& value
) noexcept {
	FAST_HELPER_BOUNDARY(
		state,
		{
			thread.reportAsSleeping();
			value = thread.getMyProcess().getIO().getInput<i64>(thread);
			thread.reportAsRunning();
			return thread.isTerminateRequested() ? Trap::Stopped : Trap::None;
		}
	)
}

vm::fast::Trap vm::fast::FastHelpers::outputU64(
	ThreadRuntimeState& state, FastVMThread& thread, u64 value
) noexcept {
	FAST_HELPER_BOUNDARY(state, {
		thread.getMyProcess().getIO().writeOutput(value);
		return Trap::None;
	})
}
