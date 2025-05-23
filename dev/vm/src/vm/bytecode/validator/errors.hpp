#pragma once
#include <base/exceptions.hpp>
#include <base/string_id.hpp>
#include <base/variant.hpp>

#include <vm/bytecode/bytecode.hpp>
#include <vm/bytecode/element_base.hpp>
#include <vm/bytecode/instructions.hpp>
#include <vm/bytecode/opcode_args.hpp>
#include <vm/bytecode/serializer/serializer.hpp>
#include <vm/bytecode/type_of_data.hpp>

#include <string_view>
#include <utility>

namespace vm::code {
	class ValidationError: public base::LogicError {
	public:
		ValidationError(std::string reason): base::LogicError(std::move(reason)) {}

		// Element causing the error.
		[[nodiscard]] virtual base::Optional<CRef<ElementBase>> maybeElement() const { return {}; }
	};

	class StackStructureMismatchError: public ValidationError {
	public:
		constexpr static const std::string_view ERR_MSG
			= "Stack structure differs between jumps and label: ";
		constexpr static const std::string_view NOTE_MSG = "One of the jumps.";
		instructions::Op_label                  label;
		std::vector<Instruction>                jumps;  /// all jumps to the label

		StackStructureMismatchError(instructions::Op_label label, std::vector<Instruction> jumps):
			  ValidationError(base::strConcat(ERR_MSG, label.arg0.label_name)),
			  label(label),
			  jumps(std::move(jumps)) {}

		[[nodiscard]] base::Optional<CRef<ElementBase>> maybeElement() const override {
			return &label;
		}
	};

	class PathWithoutEndError: public ValidationError {
	public:
		constexpr const static std::string_view ERR_MSG
			= "Not all code paths end with returns in function: ";
		const base::StrID FUNC_NAME;

		PathWithoutEndError(base::StrID func_name):
			  ValidationError(base::strConcat(ERR_MSG, func_name)),
			  FUNC_NAME(func_name) {}
	};

	/**
	 * @brief position-less error for function definitions.
	 * For function name arguments, like in call instructions, use UnknownFunctionError.
	 */
	class MissingFunctionalTypeError: public ValidationError {
	public:
		constexpr const static std::string_view ERR_MSG = "Functional type is not declared for: ";
		const base::StrID                       FUNC_NAME;

		MissingFunctionalTypeError(base::StrID func_name):
			  ValidationError(base::strConcat(ERR_MSG, func_name)),
			  FUNC_NAME(func_name) {}
	};

	/**
	 * @brief position-less error for function definitions.
	 * For function name arguments, like in call instructions, use UnknownFunctionError.
	 */
	class TypeIsNotFunctionalError: public ValidationError {
	public:
		constexpr const static std::string_view ERR_MSG = "Type is not functional: ";
		const base::StrID                       TYPE_NAME;

		TypeIsNotFunctionalError(base::StrID type_name):
			  ValidationError(base::strConcat(ERR_MSG, type_name)),
			  TYPE_NAME(type_name) {}
	};

#define DEFINE_DUPLICATED_ELEMENT_ERROR(NAME, ELEMENT_TYPE, ERROR)                      \
	class NAME: public ValidationError {                                                \
	public:                                                                             \
		constexpr static const std::string_view ERR_MSG = ERROR;                        \
		const ELEMENT_TYPE                      NEW_ELEMENT;                            \
		const ELEMENT_TYPE                      PREVIOUS_ELEMENT;                       \
                                                                                        \
		NAME(ELEMENT_TYPE new_element, ELEMENT_TYPE previous_element):                  \
			  ValidationError(ERR_MSG.data()),                                          \
			  NEW_ELEMENT(std::move(new_element)),                                      \
			  PREVIOUS_ELEMENT(std::move(previous_element)) {}                          \
                                                                                        \
		[[nodiscard]] base::Optional<CRef<ElementBase>> maybeElement() const override { \
			return &NEW_ELEMENT.name;                                                   \
		}                                                                               \
	}

	DEFINE_DUPLICATED_ELEMENT_ERROR(
		DuplicatedGlobalDataError, code::GlobalData, "Duplicated global data: "
	);

	DEFINE_DUPLICATED_ELEMENT_ERROR(DuplicatedFunctionError, code::Function, "Duplicated function: ");

	class DuplicatedTypeError: public ValidationError {
	public:
		constexpr static const std::string_view ERR_MSG = "Duplicated type: ";
		const code::TypeOfData                  NEW_ELEMENT;
		const code::TypeOfData                  PREVIOUS_ELEMENT;

		DuplicatedTypeError(code::TypeOfData new_element, code::TypeOfData previous_element):
			  ValidationError(ERR_MSG.data()),
			  NEW_ELEMENT(std::move(new_element)),
			  PREVIOUS_ELEMENT(std::move(previous_element)) {}

		[[nodiscard]] base::Optional<CRef<ElementBase>> maybeElement() const override {
			return VISIT(NEW_ELEMENT, type, return static_cast<const ElementBase*>(&type));
		}
	};

	class TypeErrorBase: public ValidationError {
	public:
		const TypeOfData TYPE;

		TypeErrorBase(std::string msg, TypeOfData type):
			  ValidationError(std::move(msg)),
			  TYPE(std::move(type)) {}

		[[nodiscard]] base::Optional<CRef<ElementBase>> maybeElement() const override {
			return VISIT(TYPE, tp, return static_cast<CRef<ElementBase>>(&tp));
		}
	};

	class InstructionErrorBase: public ValidationError {
	public:
		const Instruction INSTRUCTION;

		InstructionErrorBase(std::string_view msg, Instruction instruction):
			  ValidationError(std::string(msg)),
			  INSTRUCTION(instruction) {}

		[[nodiscard]] base::Optional<CRef<ElementBase>> maybeElement() const override {
			return VISIT(INSTRUCTION, tp, return static_cast<CRef<ElementBase>>(&tp));
		}
	};

	class ArgumentErrorBase: public ValidationError {
	public:
		const opargs::OpCodeArg ARGUMENT;

		ArgumentErrorBase(std::string msg, opargs::OpCodeArg argument):
			  ValidationError(std::move(msg)),
			  ARGUMENT(argument) {}

		[[nodiscard]] base::Optional<CRef<ElementBase>> maybeElement() const override {
			return VISIT(ARGUMENT, tp, return static_cast<CRef<ElementBase>>(&tp));
		}
	};

	class TypeWithAttributeBase: public ValidationError {
	public:
		const TypeOfData  TYPE;
		const base::StrID ATTRIBUTE_NAME;

		TypeWithAttributeBase(std::string msg, TypeOfData argument, base::StrID field_name):
			  ValidationError(std::move(msg)),
			  TYPE(std::move(argument)),
			  ATTRIBUTE_NAME(field_name) {}

		[[nodiscard]] base::Optional<CRef<ElementBase>> maybeElement() const override {
			return VISIT(TYPE, tp, return static_cast<CRef<ElementBase>>(&tp));
		}
	};

#define DEFINE_TYPE_ERROR(error_name, msg)                                                   \
	class error_name: public TypeErrorBase {                                                 \
	public:                                                                                  \
		constexpr static const std::string_view ERR_MSG = (msg);                             \
                                                                                             \
		error_name(TypeOfData type):                                                         \
			  TypeErrorBase(                                                                 \
				  base::strConcat(ERR_MSG, VISIT(type, tp, return tp.name)), std::move(type) \
			  ) {}                                                                           \
	};

#define DEFINE_INSTRUCTION_ERROR(error_name, msg)                                          \
	class error_name: public InstructionErrorBase {                                        \
	public:                                                                                \
		constexpr static const std::string_view ERR_MSG = (msg);                           \
                                                                                           \
		error_name(Instruction instruction): InstructionErrorBase(ERR_MSG, instruction) {} \
	};

#define DEFINE_ARGUMENT_ERROR(error_name, msg)                                                     \
	class error_name: public ArgumentErrorBase {                                                   \
	public:                                                                                        \
		constexpr static const std::string_view ERR_MSG = (msg);                                   \
                                                                                                   \
		error_name(opargs::OpCodeArg argument):                                                    \
			  ArgumentErrorBase(base::strConcat(ERR_MSG, argumentToString(argument)), argument) {} \
	};

#define DEFINE_TYPE_WITH_ATTRIBUTE_ERROR(error_name, msg)                           \
	class error_name: public TypeWithAttributeBase {                                \
	public:                                                                         \
		constexpr static const std::string_view ERR_MSG = (msg);                    \
                                                                                    \
		error_name(TypeOfData type, base::StrID field_name):                        \
			  TypeWithAttributeBase(                                                \
				  base::strConcat(ERR_MSG, field_name), std::move(type), field_name \
			  ) {}                                                                  \
	};

	DEFINE_TYPE_ERROR(
		InvalidImplementsError,
		"This object can implement only existing interfaces other than itself: "
	);
	DEFINE_TYPE_ERROR(EmptyVariantError, "This variant type is empty: ");
	DEFINE_TYPE_ERROR(
		DuplicatedImplementsError,
		"This interface/class tried implementing the same interface twice: "
	);
	DEFINE_TYPE_ERROR(
		InvalidExtendsError, "This class can extend only existing classes other than itself: "
	);
	DEFINE_TYPE_ERROR(
		CycleInHierarchyError, "This interface/class is a part of an inheritance cycle: "
	);
	DEFINE_TYPE_WITH_ATTRIBUTE_ERROR(
		MethodTypeError,
		"Implementations and virtual method declarations should have the same signature: "
	);
	DEFINE_TYPE_WITH_ATTRIBUTE_ERROR(
		MethodFirstArgumentError, "Methods first argument should be a this*: "
	);

	DEFINE_TYPE_WITH_ATTRIBUTE_ERROR(DuplicatedFieldError, "This objects' field is duplicated: ");
	DEFINE_TYPE_WITH_ATTRIBUTE_ERROR(
		DuplicatedVirtualMethodError, "This objects' virtual method declaration is duplicated: "
	);
	DEFINE_TYPE_WITH_ATTRIBUTE_ERROR(
		DuplicatedVirtualMethodImplementationError,
		"This objects' virtual method implementation is duplicated: "
	);
	DEFINE_TYPE_WITH_ATTRIBUTE_ERROR(
		UnimplementedVirtualMethodError,
		"This virtual method is unimplemented in an instantiable class: "
	);
	DEFINE_TYPE_WITH_ATTRIBUTE_ERROR(
		VirtualMethodSignatureError, "This class implements a method with wrong signature: "
	);
	DEFINE_TYPE_WITH_ATTRIBUTE_ERROR(
		InvalidVirtualMethodImplementationError,
		"Method implementation lacks it's declaration as a virtual method: "
	);
	DEFINE_TYPE_WITH_ATTRIBUTE_ERROR(UnknownSubtypeError, "This subtype is not defined anywhere: ");

	DEFINE_INSTRUCTION_ERROR(
		InvalidUpcastError, "The source type does not inherit from the destination type"
	);
	DEFINE_INSTRUCTION_ERROR(
		InvalidInstructionExtensionError, "The preceding instruction cannot be extended this way"
	);
	DEFINE_INSTRUCTION_ERROR(RetValDeinitError, "The return value cannot be deinitialized.")
	DEFINE_INSTRUCTION_ERROR(ArgumentMismatchError, "Instruction arguments have different types.")

	DEFINE_ARGUMENT_ERROR(UnknownTypeError, "Unknown type: ");
	DEFINE_ARGUMENT_ERROR(UnknownLocalNameError, "Unknown local name: ");
	DEFINE_ARGUMENT_ERROR(DuplicatedLocalNameError, "Duplicated local name: ");
	DEFINE_ARGUMENT_ERROR(UnknownLabelError, "Unknown label: ");
	DEFINE_ARGUMENT_ERROR(DuplicatedLabelError, "Duplicated label: ");
	DEFINE_ARGUMENT_ERROR(UnknownFunctionError, "Unknown function: ");
	DEFINE_ARGUMENT_ERROR(UnknownMethodError, "Unknown method: ");
	DEFINE_ARGUMENT_ERROR(
		InvalidFunctionCallArgumentsError,
		"Invalid function call arguments. Values on the stack do not have proper types for "
		"calling: "
	);
	DEFINE_ARGUMENT_ERROR(
		InvalidTailcallSignatureError,
		"The callee must have the same signature as the caller when tailcalling: "
	);
	DEFINE_ARGUMENT_ERROR(
		InvalidTailcallArgumentsError,
		"Invalid tailcall arguments. The stack should contain exactly ret_val and arguments for "
		"calling: "
	);
	DEFINE_ARGUMENT_ERROR(UninstantiableValueError, "Cannot instantiate a value of type: ");
	DEFINE_ARGUMENT_ERROR(InvalidArgumentSizeError, "Invalid instruction argument size: ");
	DEFINE_ARGUMENT_ERROR(InvalidArgumentTypeError, "Invalid instruction argument type: ");
	DEFINE_ARGUMENT_ERROR(TypeIsNotDataError, "Invalid instruction argument type: ");
	DEFINE_ARGUMENT_ERROR(UnknownGlobalNameError, "Unknown global name: ");
	DEFINE_ARGUMENT_ERROR(UnknownFieldError, "Given data does not contain this field: ");

}
