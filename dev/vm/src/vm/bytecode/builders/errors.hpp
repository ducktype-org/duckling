#pragma once
#include <base/exceptions.hpp>
#include <base/string_id.hpp>
#include <base/variant.hpp>

#include <vm/bytecode/instructions.hpp>
#include <vm/bytecode/opcode_args.hpp>
#include <vm/bytecode/serializer/serializer.hpp>
#include <vm/bytecode/type_of_data.hpp>

#include <string_view>
#include <utility>

namespace vm::code::builders {
	class BuilderError: public base::LogicError {
	public:
		BuilderError(std::string reason): base::LogicError(std::move(reason)) {}
	};

	class StackStructureMismatchError: public BuilderError {
	public:
		constexpr static const std::string_view ERR_MSG
			= "Stack structure differs between jumps and label.";
		const instructions::Op_label LABEL;
		std::vector<Instruction>     jumps;  /// all jumps to the label

		StackStructureMismatchError(instructions::Op_label label, std::vector<Instruction> jumps):
			  BuilderError(base::strConcat(ERR_MSG, label.arg0.label_name)),
			  LABEL(label),
			  jumps(std::move(jumps)) {}
	};

	class InvalidFunctionEndError: public BuilderError {
	public:
		constexpr const static std::string_view ERR_MSG
			= "Not all code paths end with returns in function: ";
		const base::StrID FUNC_NAME;

		InvalidFunctionEndError(base::StrID func_name):
			  BuilderError(base::strConcat(ERR_MSG, func_name)),
			  FUNC_NAME(func_name) {}
	};

	// This is a position-less error for function definitions.
	// For function name arguments, like in call instructions, use UnknownFunctionError.
	class MissingFunctionalTypeError: public BuilderError {
	public:
		constexpr const static std::string_view ERR_MSG = "Functional type is not declared for: ";
		const base::StrID                       FUNC_NAME;

		MissingFunctionalTypeError(base::StrID func_name):
			  BuilderError(base::strConcat(ERR_MSG, func_name)),
			  FUNC_NAME(func_name) {}
	};

	// This is a position-less error for function definitions.
	// For function name arguments, like in call instructions, use UnknownFunctionError.
	class TypeIsNotFunctionalError: public BuilderError {
	public:
		constexpr const static std::string_view ERR_MSG = "Type is not functional: ";
		const base::StrID                       TYPE_NAME;

		TypeIsNotFunctionalError(base::StrID type_name):
			  BuilderError(base::strConcat(ERR_MSG, type_name)),
			  TYPE_NAME(type_name) {}
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

	class TypeValidationError: public BuilderError {
	public:
		const TypeOfData TYPE;

		TypeValidationError(std::string msg, TypeOfData type):
			  BuilderError(std::move(msg)),
			  TYPE(std::move(type)) {}
	};

	class InstructionValidationError: public BuilderError {
	public:
		const Instruction INSTRUCTION;

		InstructionValidationError(std::string_view msg, Instruction instruction):
			  BuilderError(std::string(msg)),
			  INSTRUCTION(instruction) {}
	};

	class ArgumentValidationError: public BuilderError {
	public:
		const opargs::OpCodeArg ARGUMENT;

		ArgumentValidationError(std::string msg, opargs::OpCodeArg argument):
			  BuilderError(std::move(msg)),
			  ARGUMENT(argument) {}
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

#define DEFINE_INSTRUCTION_VALIDATION_ERROR(error_name, msg)                                     \
	class error_name: public InstructionValidationError {                                        \
	public:                                                                                      \
		constexpr static const std::string_view ERR_MSG = (msg);                                 \
                                                                                                 \
		error_name(Instruction instruction): InstructionValidationError(ERR_MSG, instruction) {} \
	};

#define DEFINE_ARGUMENT_VALIDATION_ERROR(error_name, msg)                        \
	class error_name: public ArgumentValidationError {                           \
	public:                                                                      \
		constexpr static const std::string_view ERR_MSG = (msg);                 \
                                                                                 \
		error_name(opargs::OpCodeArg argument):                                  \
			  ArgumentValidationError(                                           \
				  base::strConcat(ERR_MSG, argumentToString(argument)), argument \
			  ) {}                                                               \
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
	DEFINE_TYPE_VALIDATION_ERROR(DuplicatedTypeError, "Duplicated type: ");

	DEFINE_INSTRUCTION_VALIDATION_ERROR(
		InvalidUpcastError, "The source type does not inherit from the destination type"
	);
	DEFINE_INSTRUCTION_VALIDATION_ERROR(
		InvalidInstructionExtensionError, "The preceding instruction cannot be extended this way"
	);
	DEFINE_INSTRUCTION_VALIDATION_ERROR(RetValDeinitError, "The return value cannot be deinitalised.")

	DEFINE_ARGUMENT_VALIDATION_ERROR(UnknownTypeError, "Unknown type: ");
	DEFINE_ARGUMENT_VALIDATION_ERROR(UnknownLocalNameError, "Unknown local name: ");
	DEFINE_ARGUMENT_VALIDATION_ERROR(DuplicatedLocalNameError, "Duplicated local name: ");
	DEFINE_ARGUMENT_VALIDATION_ERROR(UnknownLabelError, "Unknown label: ");
	DEFINE_ARGUMENT_VALIDATION_ERROR(DuplicatedLabelError, "Duplicated label: ");
	DEFINE_ARGUMENT_VALIDATION_ERROR(UnknownFunctionError, "Unknown function: ");
	DEFINE_ARGUMENT_VALIDATION_ERROR(
		InvalidFunctionCallArgumentsError,
		"Invalid function call arguments. Values on the stack do not have proper types."
	);
	DEFINE_ARGUMENT_VALIDATION_ERROR(
		UninstantiableValueError, "Cannot intiantiate a value of this type."
	);
}
