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
#include <vm/core/process/type_metadata/type_metadata.hpp>

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
	class MissingGlobalCtorDtorError: public ValidationError {
	public:
		constexpr const static std::string_view ERR_MSG = "Missing function declaration for ";

		MissingGlobalCtorDtorError(bool is_ctor, base::StrID func_name, base::StrID global_name):
			  ValidationError(base::strConcat(
				  ERR_MSG,
				  is_ctor ? "constructor '" : "destructor '",
				  func_name,
				  "' of global variable '",
				  global_name,
				  "'"
			  )) {}
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

	class CyclicDependencyError: public ValidationError {
	public:
		constexpr static const std::string_view ERR_MSG = "Cyclic dependency detected: ";
		const base::StrID                       TYPE_NAME;

		CyclicDependencyError(const Type& type):
			  ValidationError(base::strConcat(ERR_MSG, type.getName())),
			  TYPE_NAME(type.getName()) {}
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

	class TypeAttributeBase: public ValidationError {
	public:
		const TypeOfData  TYPE;
		const base::StrID ATTRIBUTE_NAME;

		TypeAttributeBase(std::string msg, TypeOfData argument, base::StrID field_name):
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
	}

#define DEFINE_INSTRUCTION_ERROR(error_name, msg)                                          \
	class error_name: public InstructionErrorBase {                                        \
	public:                                                                                \
		constexpr static const std::string_view ERR_MSG = (msg);                           \
                                                                                           \
		error_name(Instruction instruction): InstructionErrorBase(ERR_MSG, instruction) {} \
	}

#define DEFINE_ARGUMENT_ERROR(error_name, msg)                                                     \
	class error_name: public ArgumentErrorBase {                                                   \
	public:                                                                                        \
		constexpr static const std::string_view ERR_MSG = (msg);                                   \
                                                                                                   \
		error_name(opargs::OpCodeArg argument):                                                    \
			  ArgumentErrorBase(base::strConcat(ERR_MSG, argumentToString(argument)), argument) {} \
	}

#define DEFINE_TYPE_ATTRIBUTE_ERROR(error_name, msg)                                \
	class error_name: public TypeAttributeBase {                                    \
	public:                                                                         \
		constexpr static const std::string_view ERR_MSG = (msg);                    \
                                                                                    \
		error_name(TypeOfData type, base::StrID field_name):                        \
			  TypeAttributeBase(                                                    \
				  base::strConcat(ERR_MSG, field_name), std::move(type), field_name \
			  ) {}                                                                  \
	};

#define DEFINE_CONST_ERROR(error_name, msg) \
	class error_name: public ValidationError { \
	public: \
		constexpr static const std::string_view ERR_MSG = (msg); \
		const ConstType CONST; \
        error_name(ConstType argument): ValidationError(base::strConcat(ERR_MSG, argument.name, " ", argument.referenced_type)), \
		CONST(std::move(argument)) {}                      \
		\
		[[nodiscard]] base::Optional<CRef<ElementBase>> maybeElement() const override {\
			return static_cast<CRef<ElementBase>>(&CONST);\
		}	\
	};

	DEFINE_CONST_ERROR(
		InvalidConstReferencedTypeError, "This const type's referenced type does not exist: "
	);
	DEFINE_CONST_ERROR(
		ConstReferencingConstError, "This const type references a const, not a type: "
	);
	DEFINE_TYPE_ERROR(
		CycleInHierarchyError, "This interface/class is a part of an inheritance cycle: "
	);
	DEFINE_TYPE_ERROR(EmptyVariantError, "This variant type is empty: ");
	DEFINE_TYPE_ATTRIBUTE_ERROR(
		InvalidImplementsError,
		"This object can implement only existing interfaces other than itself: "
	);
	DEFINE_TYPE_ATTRIBUTE_ERROR(
		DuplicatedImplementsError,
		"This interface/class tried implementing the same interface twice: "
	);
	DEFINE_TYPE_ATTRIBUTE_ERROR(
		InvalidExtendsError, "This class can extend only existing classes other than itself: "
	);
	DEFINE_TYPE_ATTRIBUTE_ERROR(
		MethodTypeError,
		"Implementations and virtual method declarations should have the same signature: "
	);
	DEFINE_TYPE_ATTRIBUTE_ERROR(
		MethodFirstArgumentError,
		"Methods first argument should be a pointer to an object the method is defined for: "
	);

	DEFINE_TYPE_ATTRIBUTE_ERROR(DuplicatedFieldError, "This object's field is duplicated: ");
	DEFINE_TYPE_ATTRIBUTE_ERROR(
		DuplicatedVirtualMethodError, "This object's virtual method declaration is duplicated: "
	);
	DEFINE_TYPE_ATTRIBUTE_ERROR(
		DuplicatedVirtualMethodImplementationError,
		"This object's virtual method implementation is duplicated: "
	);
	DEFINE_TYPE_ATTRIBUTE_ERROR(
		UnimplementedVirtualMethodError,
		"This virtual method is unimplemented in an instantiable class: "
	);
	DEFINE_TYPE_ATTRIBUTE_ERROR(
		VirtualMethodSignatureError, "This class implements a method with wrong signature: "
	);
	DEFINE_TYPE_ATTRIBUTE_ERROR(
		InvalidVirtualMethodImplementationError,
		"Method implementation lacks its declaration as a virtual method: "
	);
	DEFINE_TYPE_ATTRIBUTE_ERROR(UnknownSubtypeError, "This subtype is not defined anywhere: ");

	DEFINE_INSTRUCTION_ERROR(
		InvalidUpcastError, "The source type does not inherit from the destination type"
	);
	DEFINE_INSTRUCTION_ERROR(
		InvalidInstructionExtensionError, "The preceding instruction cannot be extended this way"
	);
	DEFINE_INSTRUCTION_ERROR(RetValDeinitError, "The return value cannot be deinitialized.");
	DEFINE_INSTRUCTION_ERROR(CastSizeMismatchError, "Cannot cast to type of different size.");
	DEFINE_ARGUMENT_ERROR(UnknownTypeError, "Unknown type: ");
	DEFINE_ARGUMENT_ERROR(UnknownLocalNameError, "Unknown local name: ");
	DEFINE_ARGUMENT_ERROR(DuplicatedLocalNameError, "Duplicated local name: ");
	DEFINE_ARGUMENT_ERROR(UnknownLabelError, "Unknown label: ");
	DEFINE_ARGUMENT_ERROR(DuplicatedLabelError, "Duplicated label: ");
	DEFINE_ARGUMENT_ERROR(UnknownFunctionError, "Unknown function: ");
	DEFINE_ARGUMENT_ERROR(UnknownMethodError, "Unknown method: ");
	DEFINE_ARGUMENT_ERROR(InvalidBuiltinFunctionError, "Function is not builtin: ");
	DEFINE_ARGUMENT_ERROR(
		InvalidFunctionCallArgumentsError,
		"Invalid function call arguments. Values on the stack do not have proper types for "
		"calling: "
	);
	DEFINE_ARGUMENT_ERROR(
		InvalidTailcallSignatureError,
		"The called must have the same signature as the caller when tailcalling: "
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
	DEFINE_INSTRUCTION_ERROR(ArgumentMismatchError, "Instruction arguments have different types.");
	DEFINE_INSTRUCTION_ERROR(
		PointerTypeMismatchError, "Inner pointer type does not match expected type."
	);
	DEFINE_INSTRUCTION_ERROR(
		InvalidVirtualCallError, "Provided method does not exists for a given argument."
	);
	DEFINE_INSTRUCTION_ERROR(
		ConstFirstArgError, "First argument of a modyfing instruction is const."
	);
	DEFINE_INSTRUCTION_ERROR(
		FixedSizeTableTypeMismatchError, "Inner fixed size table type does not match expected type."
	);
	DEFINE_INSTRUCTION_ERROR(
		DynamicTableTypeMismatchError, "Inner dynamic table type does not match expected type."
	);
	DEFINE_INSTRUCTION_ERROR(
		StructTypeMismatchError, "Inner struct type does not match expected type."
	);
	DEFINE_INSTRUCTION_ERROR(
		VariantTypeMismatchError, "Possible variant types do not match expected type."
	);
	DEFINE_ARGUMENT_ERROR(UnknownGlobalNameError, "Unknown global name: ");
	DEFINE_ARGUMENT_ERROR(UnknownFieldError, "Given data does not contain this field: ");
	DEFINE_ARGUMENT_ERROR(NonPrimitiveCastError, "Cannot in-place cast to non-primitive type: ");
	DEFINE_INSTRUCTION_ERROR(
		VTableTypeMismatchError, "The vtable type does not match the object pointer type."
	);

}
