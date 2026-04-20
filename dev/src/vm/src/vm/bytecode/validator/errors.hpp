#pragma once
#include <base/except/exceptions.hpp>
#include <base/extend_cpp/variant_match.hpp>

#include <string_id/string_id.hpp>

#include <vm/bytecode/bytecode.hpp>
#include <vm/bytecode/element_base.hpp>
#include <vm/bytecode/instructions.hpp>
#include <vm/bytecode/opcode_args.hpp>
#include <vm/bytecode/serializer/serializer.hpp>
#include <vm/bytecode/type_of_data.hpp>
#include <vm/bytecode/validator/valid_type/valid_type.hpp>
#include <vm/core/safe/type_metadata/type_metadata.hpp>

#include <string_view>
#include <utility>

namespace vm::code {
	class ValidationError: public base::LogicError {
	public:
		ValidationError(const std::string& reason): LogicError(reason) {}

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
			  ValidationError(base::strConcat(ERR_MSG, label.label.label_name)),
			  label(label),
			  jumps(std::move(jumps)) {}

		[[nodiscard]] base::Optional<CRef<ElementBase>> maybeElement() const override {
			return &label;
		}
	};

	class PathWithoutEndError: public ValidationError {
	public:
		constexpr static std::string_view ERR_MSG
			= "Not all code paths end with returns in function: ";
		const base::StrID func_name;

		PathWithoutEndError(base::StrID func_name):
			  ValidationError(base::strConcat(ERR_MSG, func_name)),
			  func_name(func_name) {}
	};

	class VoidTypeArgumentError: public ValidationError {
	public:
		constexpr static std::string_view ERR_MSG
			= "Void type cannot be used as argument in function: ";
		const base::StrID func_name;

		VoidTypeArgumentError(base::StrID func_name):
			  ValidationError(base::strConcat(ERR_MSG, func_name)),
			  func_name(func_name) {}
	};

	/**
	 * @brief position-less error for function definitions.
	 * For function name arguments, like in call instructions, use UnknownFunctionError.
	 */
	class MissingFunctionalTypeError: public ValidationError {
	public:
		constexpr static std::string_view ERR_MSG = "Functional type is not declared for: ";
		const base::StrID                 func_name;

		MissingFunctionalTypeError(base::StrID func_name):
			  ValidationError(base::strConcat(ERR_MSG, func_name)),
			  func_name(func_name) {}
	};

	/**
	 * @brief position-less error for function definitions.
	 * For function name arguments, like in call instructions, use UnknownFunctionError.
	 */
	class MissingGlobalCtorDtorError: public ValidationError {
	public:
		constexpr static std::string_view ERR_MSG = "Missing function declaration for ";

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
		constexpr static std::string_view ERR_MSG = "Type is not functional: ";
		const base::StrID                 type_name;

		TypeIsNotFunctionalError(base::StrID type_name):
			  ValidationError(base::strConcat(ERR_MSG, type_name)),
			  type_name(type_name) {}
	};

	class CyclicDependencyError: public ValidationError {
	public:
		constexpr static std::string_view ERR_MSG = "Cyclic dependency detected: ";
		const TypeOfData                  type;

		CyclicDependencyError(const TypeOfData& type):
			  ValidationError(base::strConcat(ERR_MSG, typeName(type))),
			  type(type) {}

		[[nodiscard]] base::Optional<CRef<ElementBase>> maybeElement() const override {
			return VISIT(type, type, return static_cast<const ElementBase*>(&type));
		}
	};

#define DEFINE_DUPLICATED_ELEMENT_ERROR(NAME, ELEMENT_TYPE, ERROR)                      \
	class NAME: public ValidationError {                                                \
	public:                                                                             \
		constexpr static std::string_view ERR_MSG = ERROR;                              \
		const ELEMENT_TYPE                new_element;                                  \
		const ELEMENT_TYPE                previous_element;                             \
                                                                                        \
		NAME(ELEMENT_TYPE new_element, ELEMENT_TYPE previous_element):                  \
			  ValidationError(ERR_MSG.data()),                                          \
			  new_element(std::move(new_element)),                                      \
			  previous_element(std::move(previous_element)) {}                          \
                                                                                        \
		[[nodiscard]] base::Optional<CRef<ElementBase>> maybeElement() const override { \
			return &new_element.name;                                                   \
		}                                                                               \
	}

	DEFINE_DUPLICATED_ELEMENT_ERROR(
		DuplicatedGlobalDataError, code::GlobalData, "Duplicated global data: "
	);

	DEFINE_DUPLICATED_ELEMENT_ERROR(DuplicatedFunctionError, code::Function, "Duplicated function: ");
	DEFINE_DUPLICATED_ELEMENT_ERROR(
		DuplicatedExtCFunctionError, code::ExternalCFunction, "Duplicated external C function: "
	);

	/**
	 * @note A type may be trivially copyable if its bits can be just copied and its
	 * value remains correct.
	 */
	class ExtCArgumentTypeNotTriviallyCopyable: public ValidationError {
	public:
		constexpr static std::string_view ERR_MSG = "Given VM type is not trivially copyable: ";

		ExtCArgumentTypeNotTriviallyCopyable(const valid_type::ValidType& type):
			  ValidationError(base::strConcat(ERR_MSG, type.getName())) {}
	};

	class InvalidMainReturnType: public ValidationError {
	public:
		constexpr static std::string_view ERR_MSG
			= "It's required for the `main` function to return a value of type `i64`";
		const code::FuncSignature main_signature;

		InvalidMainReturnType(code::FuncSignature main_signature):
			  ValidationError(ERR_MSG.data()),
			  main_signature(std::move(main_signature)) {}

		[[nodiscard]] base::Optional<CRef<ElementBase>> maybeElement() const override {
			return static_cast<CRef<ElementBase>>(&main_signature.result_type);
		}
	};

	class DuplicatedTypeError: public ValidationError {
	public:
		constexpr static const std::string_view ERR_MSG = "Duplicated type: ";
		const code::TypeOfData                  new_element;
		const code::TypeOfData                  previous_element;

		DuplicatedTypeError(code::TypeOfData new_element, code::TypeOfData previous_element):
			  ValidationError(ERR_MSG.data()),
			  new_element(std::move(new_element)),
			  previous_element(std::move(previous_element)) {}

		[[nodiscard]] base::Optional<CRef<ElementBase>> maybeElement() const override {
			return VISIT(new_element, type, return static_cast<const ElementBase*>(&type));
		}
	};

	class TypeErrorBase: public ValidationError {
	public:
		const TypeOfData type;

		TypeErrorBase(const std::string& msg, TypeOfData type):
			  ValidationError(msg),
			  type(std::move(type)) {}

		[[nodiscard]] base::Optional<CRef<ElementBase>> maybeElement() const override {
			return VISIT(type, tp, return static_cast<CRef<ElementBase>>(&tp));
		}
	};

	class InstructionErrorBase: public ValidationError {
	public:
		const Instruction instruction;

		InstructionErrorBase(std::string_view msg, Instruction instruction):
			  ValidationError(std::string(msg)),
			  instruction(instruction) {}

		[[nodiscard]] base::Optional<CRef<ElementBase>> maybeElement() const override {
			return instruction.visit([](auto&& i) { return static_cast<CRef<ElementBase>>(&i); });
		}
	};

	class ArgumentErrorBase: public ValidationError {
	public:
		const opargs::OpCodeArg argument;

		ArgumentErrorBase(const std::string& msg, opargs::OpCodeArg argument):
			  ValidationError(msg),
			  argument(argument) {}

		[[nodiscard]] base::Optional<CRef<ElementBase>> maybeElement() const override {
			return VISIT(argument, tp, return static_cast<CRef<ElementBase>>(&tp));
		}
	};

	class TypeAttributeBase: public ValidationError {
	public:
		const TypeOfData  type;
		const base::StrID attribute_name;

		TypeAttributeBase(const std::string& msg, TypeOfData argument, base::StrID field_name):
			  ValidationError(msg),
			  type(std::move(argument)),
			  attribute_name(field_name) {}

		[[nodiscard]] base::Optional<CRef<ElementBase>> maybeElement() const override {
			return VISIT(type, tp, return static_cast<CRef<ElementBase>>(&tp));
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

	DEFINE_TYPE_ERROR(
		CycleInHierarchyError, "This interface/class is a part of an inheritance cycle: "
	);
	DEFINE_TYPE_ERROR(InvalidPrimitiveSizeError, "Primitive type cannot have size 0: ");
	DEFINE_TYPE_ERROR(
		TooFewVariantAlternativesError, "Variants should have at least two alternatives: "
	);
	DEFINE_TYPE_ATTRIBUTE_ERROR(
		InvalidImplementsError,
		"This object can implement only existing interfaces other than itself: "
	);
	DEFINE_TYPE_ATTRIBUTE_ERROR(
		DuplicatedImplementsError,
		"This interface/class tried implementing the same interface twice: "
	);
	DEFINE_TYPE_ATTRIBUTE_ERROR(
		DuplicatedMethodNameError,
		"Every method must have a deterministic signature, but this method name is used for "
		"different signatures: "
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
	DEFINE_TYPE_ATTRIBUTE_ERROR(
		DuplicatedVariantAlternativeError, "This variant alternative is duplicated"
	)

	DEFINE_INSTRUCTION_ERROR(
		InvalidUpcastError, "The source type does not inherit from the destination type"
	);
	DEFINE_INSTRUCTION_ERROR(
		InvalidDowncastError, "The source type does not inherit from the destination type"
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
		PointerTypeMismatchError, "Pointer type does not match the expected type."
	);
	DEFINE_INSTRUCTION_ERROR(FieldTypeMismatchError, "Field type does not match the expected type.");
	DEFINE_INSTRUCTION_ERROR(
		InvalidVirtualCallError, "Provided method does not exists for a given argument."
	);
	DEFINE_INSTRUCTION_ERROR(
		FixedSizeTableTypeMismatchError, "Fixed size table type does not match the expected type."
	);
	DEFINE_INSTRUCTION_ERROR(
		DynamicTableTypeMismatchError, "Dynamic table type does not match the expected type."
	);
	DEFINE_INSTRUCTION_ERROR(
		StructTypeMismatchError, "Struct type does not match the expected type."
	);
	DEFINE_INSTRUCTION_ERROR(
		VariantTypeMismatchError, "Possible variant types do not match the expected type."
	);
	DEFINE_ARGUMENT_ERROR(UnknownGlobalNameError, "Unknown global name: ");
	DEFINE_ARGUMENT_ERROR(UnknownFieldError, "Given data does not contain this field: ");
	DEFINE_ARGUMENT_ERROR(NonPrimitiveCastError, "Cannot in-place cast to non-primitive type: ");
	DEFINE_INSTRUCTION_ERROR(
		VTableTypeMismatchError, "The vtable type does not match the object pointer type."
	);
	DEFINE_INSTRUCTION_ERROR(NotAClassTypeError, "This type does not represent a class.");
	DEFINE_INSTRUCTION_ERROR(
		OpaqueTypeMismatchError, "The opaque type does not match the expected type."
	);
	DEFINE_INSTRUCTION_ERROR(VoidRetValAssignmentError, "Cannot assign to 'ret_val' of type void.");
}
