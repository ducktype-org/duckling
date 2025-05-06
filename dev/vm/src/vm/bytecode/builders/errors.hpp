#pragma once
#include <base/exceptions.hpp>
#include <base/string_id.hpp>
#include <base/variant.hpp>

#include <vm/bytecode/instructions.hpp>
#include <vm/bytecode/opcode_args.hpp>
#include <vm/bytecode/type_of_data.hpp>

#include <string_view>
#include <utility>

namespace vm::code::builders {
	class BuilderError: public base::LogicError {
	public:
		BuilderError(std::string reason): base::LogicError(std::move(reason)) {}
	};

	class DuplicatedTypeError: public BuilderError {
	public:
		constexpr const static std ::string_view ERR_MSG = "Duplicated type: ";

		DuplicatedTypeError(base::StrID name): BuilderError(base::strConcat(ERR_MSG, name)) {}
	};

	class StackStructureMismatchError: public BuilderError {
	public:
		constexpr static const std::string_view ERR_MSG
			= "Stack structure differs between jumps and label.";
		std::vector<Instruction> linked_instructions;  /// all jumps to the label and the label

		StackStructureMismatchError(std::vector<Instruction> linked_instructions):
			  BuilderError(base::strConcat(ERR_MSG)),
			  linked_instructions(std::move(linked_instructions)) {}
	};

	class MissingFunctionalTypeError: public BuilderError {
	public:
		constexpr const static std::string_view ERR_MSG = "Functional type is not declared for: ";
		const vm::opargs::FunctionName          FUNC;

		MissingFunctionalTypeError(opargs::FunctionName func):
			  BuilderError(base::strConcat(ERR_MSG, func.function_name)),
			  FUNC(func) {}
	};

	class TypeIsNotFunctionalError: public BuilderError {
	public:
		constexpr const static std::string_view ERR_MSG = "Type is not functional: ";
		const base::StrID                       TYPE_NAME;
		const vm::opargs::FunctionName          FUNC;

		TypeIsNotFunctionalError(base::StrID type_name, opargs::FunctionName func):
			  BuilderError(base::strConcat(ERR_MSG, type_name)),
			  TYPE_NAME(type_name),
			  FUNC(func) {}
	};

	class UnknownSubtypeError: public BuilderError {
	public:
		constexpr static const std::string_view ERR_MSG = "This subtype is not defined anywhere: ";
		const TypeOfData                        BASE_TYPE;
		const base::StrID                       MISSING_NAME;

		UnknownSubtypeError(TypeOfData base_type, base::StrID missing_name):
			  BuilderError(base::strConcat(ERR_MSG, missing_name)),
			  BASE_TYPE(std::move(base_type)),
			  MISSING_NAME(missing_name) {}
	};

	class UnknownTypeError: public BuilderError {
	public:
		constexpr static const std::string_view ERR_MSG = "Unknown type: ";
		const vm::opargs::Type                  TYPE;

		UnknownTypeError(vm::opargs::Type type):
			  BuilderError(base::strConcat(ERR_MSG, type.type_name)),
			  TYPE(type) {}
	};

	class DuplicateLocalNameError: public BuilderError {
	public:
		constexpr static const std::string_view ERR_MSG = "Duplicate local name: ";
		const vm::opargs::StackLocalAny         NAME;

		DuplicateLocalNameError(vm::opargs::StackLocalAny name):
			  BuilderError(base::strConcat(ERR_MSG, name.var_name)),
			  NAME(name) {}
	};

	class InvalidLocalNameError: public BuilderError {
	public:
		constexpr static const std::string_view ERR_MSG = "This local does not exist: ";
		const vm::opargs::OpCodeLocalArg        NAME;

		InvalidLocalNameError(vm::opargs::OpCodeLocalArg name):
			  BuilderError(base::strConcat(ERR_MSG, VISIT(name, n, return n.var_name))),
			  NAME(name) {}
	};

	class EmptyStackDeinitError: public BuilderError {
	public:
		constexpr const static std::string_view ERR_MSG = "Popping from empty variable stack.";

		EmptyStackDeinitError(): BuilderError(base ::strConcat(ERR_MSG)) {}
	};

	class BadReturnError: public BuilderError {
	public:
		constexpr const static std ::string_view ERR_MSG
			= "Function returns, but incorrect return type is on the stack\'s bottom";

		BadReturnError(): BuilderError(base ::strConcat(ERR_MSG)) {}
	};

	class InvalidFunctionCallArguments: public BuilderError {
	public:
		constexpr const static std ::string_view ERR_MSG
			= "Invalid function call arguments. Values on the stack do not have proper types.";

		InvalidFunctionCallArguments(): BuilderError(base ::strConcat(ERR_MSG)) {}
	};

	class TypeValidationError: public BuilderError {
	public:
		const TypeOfData TYPE;

		TypeValidationError(std::string msg, TypeOfData type):
			  BuilderError(std::move(msg)),
			  TYPE(std::move(type)) {}
	};

	class InstructionValidationError: public BuilderError {
	public:
		InstructionValidationError(std::string_view msg): BuilderError(std::string(msg)) {}
	};

#define DEFINE_TYPE_VALIDATION_ERROR(error_name, msg)                                        \
	class error_name: public TypeValidationError {                                           \
	public:                                                                                  \
		constexpr static const std::string_view ERR_MSG = (msg);                             \
                                                                                             \
		error_name(TypeOfData type):                                                         \
			  TypeValidationError(                                                           \
				  base::strConcat(ERR_MSG, VISIT(type, tp, return tp.name)), std::move(type) \
			  ) {}                                                                           \
	};

#define DEFINE_INSTRUCTION_VALIDATION_ERROR(error_name, msg)     \
	class error_name: public InstructionValidationError {        \
	public:                                                      \
		constexpr static const std::string_view ERR_MSG = (msg); \
                                                                 \
		error_name(): InstructionValidationError(ERR_MSG) {}     \
	};

	DEFINE_TYPE_VALIDATION_ERROR(
		InvalidImplementsError, "This interface/class can implement only other interfaces: "
	);
	DEFINE_TYPE_VALIDATION_ERROR(InvalidExtends, "This class can extend only other classes: ");
	DEFINE_TYPE_VALIDATION_ERROR(
		MissingAncestorFieldError, "This class does not contain all of its ancestors' fields: "
	);
	DEFINE_TYPE_VALIDATION_ERROR(
		CycleInHierarchyError, "This inerface/class is a part of an inheritance cycle: "
	);

	DEFINE_INSTRUCTION_VALIDATION_ERROR(
		UninstantiableValueError, "Cannot intiantiate a value of this type."
	);
	DEFINE_INSTRUCTION_VALIDATION_ERROR(
		InvalidUpcastError, "The source type does not inherit from the destination type"
	);
	DEFINE_INSTRUCTION_VALIDATION_ERROR(
		InvalidInstructionExtensionError, "The preceding instruction cannot be extended this way"
	);
}
