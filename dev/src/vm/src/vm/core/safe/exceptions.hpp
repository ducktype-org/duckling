// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once
#include <base/except/exceptions.hpp>

namespace vm::exceptions {
	class VMRuntimeException: public base::Exception {
		std::string message;

	public:
		VMRuntimeException(std::string message): message(std::move(message)) {}

		[[nodiscard]]
		const char* what() const noexcept override {
			return message.c_str();
		}
	};

#define VM_RUNTIME_EXCEPTION(name, msg)                     \
	struct name final: public VMRuntimeException {          \
		constexpr static std::string_view ERR_MSG = msg;    \
		name(): VMRuntimeException(std::string(ERR_MSG)) {} \
	}

	VM_RUNTIME_EXCEPTION(VMPanicException, "Program panicked");
	VM_RUNTIME_EXCEPTION(VMNullPointerAccessException, "Accessing null pointer");
	VM_RUNTIME_EXCEPTION(VMVtableUnset, "Calling a virtual method with an unset vtable");
	VM_RUNTIME_EXCEPTION(VMOutOfBlockBoundsException, "Accessing block out of bounds");
	VM_RUNTIME_EXCEPTION(VMUseAfterFreeException, "Data was freed");
	VM_RUNTIME_EXCEPTION(VMDoubleFreeException, "Data was already freed");
	VM_RUNTIME_EXCEPTION(
		VMInvalidFreeException,
		"Only whole heap allocations can be freed by allocator-returned pointer"
	);
	VM_RUNTIME_EXCEPTION(VMStackOverflowException, "VM stack overflow");
	VM_RUNTIME_EXCEPTION(VMResumedWithPausedStatusException, "Resumed with paused status");
	VM_RUNTIME_EXCEPTION(VMZeroDivisionException, "Tried dividing by zero");
	VM_RUNTIME_EXCEPTION(VMFoundMemoryLeakException, "Memory leak detected");
	VM_RUNTIME_EXCEPTION(VMMemoryAllocationError, "Failed to allocate memory");
	// @TODO: #1431 remove this
	VM_RUNTIME_EXCEPTION(VMGlobalNotFoundException, "Global variable not found");
	VM_RUNTIME_EXCEPTION(VMDeadlockException, "Deadlock detected");
	VM_RUNTIME_EXCEPTION(VMDestroyLockedMutexException, "Destroying a locked mutex");
	VM_RUNTIME_EXCEPTION(
		VMDynTableReAllocTypeMismatch, "dynTableReAlloc called on a non-dynamic-table block"
	);

#define VM_RUNTIME_EXCEPTION_WITH_PARAM(name, msg, type)                 \
	struct name final: public VMRuntimeException {                       \
		type                              value;                         \
		constexpr static std::string_view ERR_MSG = msg;                 \
		name(const type& value):                                         \
			  VMRuntimeException(base::strConcat(ERR_MSG, ": ", value)), \
			  value(value) {}                                            \
	}

	VM_RUNTIME_EXCEPTION_WITH_PARAM(VMResourceDoesNotExist, "Resource does not exist", std::string);
	VM_RUNTIME_EXCEPTION_WITH_PARAM(VMFFIError, "FFI error", std::string);
}
