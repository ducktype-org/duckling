#pragma once
#include <base/exceptions.hpp>
#include <base/string_id.hpp>

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
		const base::StrID                       FUNC_NAME;

		MissingFunctionalTypeError(base::StrID func_name):
			  BuilderError(base::strConcat(ERR_MSG, func_name)),
			  FUNC_NAME(func_name) {}
	};

	class TypeIsNotFunctionalError: public BuilderError {
	public:
		constexpr const static std::string_view ERR_MSG = "Type is not functional: ";

		TypeIsNotFunctionalError(base::StrID type_name):
			  BuilderError(base::strConcat(ERR_MSG, type_name)) {}
	};

	class MissingSubtypeError: public BuilderError {
	public:
		constexpr static const std::string_view ERR_MSG = "This subtype is not defined anywhere: ";
		const TypeOfData                        BASE_TYPE;
		const base::StrID                       MISSING_NAME;

		MissingSubtypeError(TypeOfData base_type, base::StrID missing_name):
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

	class MissingVTablePtrError: public BuilderError {
	public:
		constexpr const static std ::string_view ERR_MSG = "Missing VTable pointer in: ";

		MissingVTablePtrError(base::StrID name): BuilderError(base::strConcat(ERR_MSG, name)) {}
	};
}
