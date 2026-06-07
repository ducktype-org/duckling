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
	struct name: public VMRuntimeException {                \
		constexpr static std::string_view ERR_MSG = msg;    \
		name(): VMRuntimeException(std::string(ERR_MSG)) {} \
	}

	VM_RUNTIME_EXCEPTION(VMNullPointerCopyException, "Copying to/from null pointer");
	VM_RUNTIME_EXCEPTION(VMNullPointerAccessException, "Accessing null pointer");
	VM_RUNTIME_EXCEPTION(VMVtableUnset, "Calling a virtual method with an unset vtable");
	VM_RUNTIME_EXCEPTION(VMOutOfBlockBoundsException, "Accessing block out of bounds");
	VM_RUNTIME_EXCEPTION(VMUseAfterFreeException, "Data was freed");
	VM_RUNTIME_EXCEPTION(VMStackOverflowException, "VM stack overflow");
	VM_RUNTIME_EXCEPTION(VMResumedWithPausedStatusException, "Resumed with paused status");
	VM_RUNTIME_EXCEPTION(VMZeroDivisionException, "Tried dividing by zero");
	VM_RUNTIME_EXCEPTION(VMFoundMemoryLeakException, "Memory leak detected");
	VM_RUNTIME_EXCEPTION(VMMemoryAllocationError, "Failed to allocate memory");
	// @TODO: #1431 remove this
	VM_RUNTIME_EXCEPTION(VMGlobalNotFoundException, "Global variable not found");
	struct VMDataRaceException : public VMRuntimeException {
		constexpr static std::string_view ERR_MSG = "[FastTrack] Data race detected";
		VMDataRaceException() : VMRuntimeException(std::string(ERR_MSG)) {}
		explicit VMDataRaceException(std::string detail) :
			VMRuntimeException(std::string(ERR_MSG) + ": " + std::move(detail)) {}
	};

#define VM_RUNTIME_EXCEPTION_WITH_PARAM(name, msg, type)                 \
	struct name: public VMRuntimeException {                             \
		type                              value;                         \
		constexpr static std::string_view ERR_MSG = msg;                 \
		name(const type& value):                                         \
			  VMRuntimeException(base::strConcat(ERR_MSG, ": ", value)), \
			  value(value) {}                                            \
	}

	VM_RUNTIME_EXCEPTION_WITH_PARAM(VMResourceDoesNotExist, "Resource does not exist", std::string);
}
