/**
 * @brief This file define the instruction structures and instruction union
 * given argument type definitions.
 * It is needed to include "instruction_id.hpp" before this file, which defines the instruction IDs,
 * and define the argument types and verify them with "argument_definitions.hpp".
 */
// NOLINTBEGIN
#ifndef ARG_NAMESPACE
#define ARG_NAMESPACE_DEFAULT
#define ARG_NAMESPACE
#endif

#define HANDLE_ARG(type, name) ARG_NAMESPACE type name;

#define HANDLE_INSTR_ARGS(NAME, ...)             \
	struct NAME {                                \
		FOR_EACH(HANDLE_ARG EXPAND, __VA_ARGS__) \
	};                                           \
	static_assert(sizeof(NAME) <= 16);

#include "instruction_definitions.hpp"
#undef HANDLE_ARG
#undef HANDLE_INSTR_ARGS

struct Instruction {
	InstrID id;

	union {
#define HANDLE_INSTR(NAME) NAME instr_##NAME;
#include "instruction_definitions.hpp"
#undef HANDLE_INSTR
	};
};

#ifdef ARG_NAMESPACE_DEFAULT
#undef ARG_NAMESPACE_DEFAULT
#undef ARG_NAMESPACE
#endif

// NOLINTEND
