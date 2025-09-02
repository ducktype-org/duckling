#pragma once
#include <base/exceptions.hpp>

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
	VM_RUNTIME_EXCEPTION(VMOutOfBlockBoundsException, "Accessing block out of bounds");
	VM_RUNTIME_EXCEPTION(VMUseAfterFreeException, "Data was freed");
	VM_RUNTIME_EXCEPTION(VMStackOverflowException, "VM stack overflow");
	VM_RUNTIME_EXCEPTION(VMResumedWithPausedStatusException, "Resumed with paused status");
	VM_RUNTIME_EXCEPTION(VMNegativeOffsetException, "Moving offset to negative value");
	VM_RUNTIME_EXCEPTION(VMZeroDivisionException, "Tried dividing by zero");
	VM_RUNTIME_EXCEPTION(VMFoundMemoryLeakException, "The last executed instruction has caused a memory leak");
}
