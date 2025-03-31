#pragma once
#include "base/string_id.hpp"
#include "vm/code/opcode_args.hpp"
#include "vm/code/type_of_data.hpp"
#include <base/exceptions.hpp>
#include <string_view>
#include <utility>

#define EMPTY
#define DEF_BUILDER_ERR(name, err_msg, cons_args, super_args)                 \
	class name: public BuilderError {                                         \
	public:                                                                   \
		constexpr static std::string_view ERR_MSG = err_msg;                  \
		name(cons_args): BuilderError(base::strConcat(ERR_MSG super_args)) {} \
	}

namespace vm::code::builders {
	class BuilderError: public base::LogicError {
	public:
		BuilderError(std::string reason): base::LogicError(std::move(reason)) {}
	};

	DEF_BUILDER_ERR(DuplicatedTypeError, "Duplicated type: ", base::StrID name, COMMA name);

	DEF_BUILDER_ERR(StackStateError, "Stack state differs between jumps.", EMPTY, EMPTY);

	DEF_BUILDER_ERR(
		MissingFunctionalTypeError,
		"Functional type is not declared for: ",
		base::StrID name,
		COMMA       name
	);

	DEF_BUILDER_ERR(
		TypeIsNotFunctionalError, "Type is not functional: ", base::StrID name, COMMA name
	);

	class MissingSubtypeError: public BuilderError {
	public:
		constexpr static std ::string_view ERR_MSG = "This subtype is not defined anywhere: ";
		const TypeOfData                   BASE_TYPE;
		const base::StrID                  MISSING_NAME;

		MissingSubtypeError(TypeOfData base_type, base::StrID missing_name):
			  BuilderError(base::strConcat(ERR_MSG, missing_name)),
			  BASE_TYPE(std::move(base_type)),
			  MISSING_NAME(missing_name) {}
	};

	class UnknownTypeError: public BuilderError {
	public:
		constexpr static std ::string_view ERR_MSG = "Unknown type: ";
		const vm::opargs::Type             TYPE;

		UnknownTypeError(vm::opargs::Type type):
			  BuilderError(base::strConcat(ERR_MSG, type.type_name)),
			  TYPE(type) {}
	};
}

#undef DEF_BUILDER_ERR
#undef EMPTY
