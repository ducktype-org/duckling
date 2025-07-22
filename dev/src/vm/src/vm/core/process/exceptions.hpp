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

	VM_RUNTIME_EXCEPTION(VMNullPointerCopyException, "Null pointer copy exception");
	VM_RUNTIME_EXCEPTION(VMNullPointerAccessException, "Null pointer access exception");
	VM_RUNTIME_EXCEPTION(VMOutOfBlockBoundsException, "Out of block bounds exception");
	VM_RUNTIME_EXCEPTION(VMDoubleFreeException, "Double free exception");
	VM_RUNTIME_EXCEPTION(VMUseAfterFreeException, "Use after free exception");
	VM_RUNTIME_EXCEPTION(VMStackOverflowException, "Stack overflow exception");
	// @TODO: add operator to constructor
	VM_RUNTIME_EXCEPTION(VMUnknownOperatorException, "Unknown operator exception");
	VM_RUNTIME_EXCEPTION(VMUnexpectedExecutionStatus, "Unexpected execution status exception");
	VM_RUNTIME_EXCEPTION(VMInvalidBuiltinFunctionException, "Invalid builtin function exception");
	VM_RUNTIME_EXCEPTION(VMInvalidBuiltinArgumentsException, "Invalid builtin arguments exception");
	VM_RUNTIME_EXCEPTION(VMResuemedWithPausedStatusException, "Resumed with paused status exception");
	VM_RUNTIME_EXCEPTION(VMNegativeOffsetException, "Negative offset exception");
	VM_RUNTIME_EXCEPTION(
		VMUnreferencedBlockDeletionException, "Unreferenced block deletion exception"
	);
	VM_RUNTIME_EXCEPTION(VMExtL64NotConsumedException, "ext_l64 not consumed exception");
	VM_RUNTIME_EXCEPTION(VMExtTypeNotConsumedException, "ext_type not consumed exception");
	VM_RUNTIME_EXCEPTION(VMExtFieldNotConsumedException, "ext_field not consumed exception");
	VM_RUNTIME_EXCEPTION(VMExtTypeFieldNotConsumedException, "ext_type_field not found exception");
	VM_RUNTIME_EXCEPTION(VMExtTypeL64NotConsumedException, "ext_type_l64 not consumed exception");
	VM_RUNTIME_EXCEPTION(VMExcutingLabelException, "Handling label should not be possible");


}
