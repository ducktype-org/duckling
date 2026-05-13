/**
 * @brief This file define the instruction structures and instruction union
 * given argument type definitions. It also defines instruction maker functions for easier
 * instruction creation.
 * You need to include "ids.hpp" before this file, which defines the
 * instruction IDs, and define the argument types and verify them with "argument_definitions.hpp".
 * Example usages are in "relocatable.hpp" and "executable.hpp".
 */
// NOLINTBEGIN

namespace instr_structs {

#ifndef ARG_NAMESPACE
	#define ARG_NAMESPACE_DEFAULT
	#define ARG_NAMESPACE
#endif

#define _DETAIL_CREATE_MEMBER(type, name) ARG_NAMESPACE type name;
#define HANDLE_INSTR_ARGS(NAME, ...)                        \
	struct NAME {                                           \
		FOR_EACH(_DETAIL_CREATE_MEMBER EXPAND, __VA_ARGS__) \
	};                                                      \
	static_assert(sizeof(NAME) <= 16);

#include "instruction_definitions.hpp"
#undef _DETAIL_CREATE_MEMBER
#undef HANDLE_INSTR_ARGS
}

struct Instruction {
	InstrID id;

	union {
#define HANDLE_INSTR(NAME) instr_structs::NAME CAT(instr_, NAME);
#include "instruction_definitions.hpp"
#undef HANDLE_INSTR
	};
};

namespace maker {
#define HANDLE_ARG(type, name)       ARG_NAMESPACE type name,
#define HANDLE_ARG_LAST(type, name)  ARG_NAMESPACE type name
#define HANDLE_CONS(type, name)      std::move(name),
#define HANDLE_CONS_LAST(type, name) std::move(name)
#define HANDLE_INSTR_ARGS(NAME, ...)                                                       \
	constexpr Instruction NAME(                                                            \
		FOR_EACH_CUSTOM_LAST(HANDLE_ARG EXPAND, HANDLE_ARG_LAST EXPAND, __VA_ARGS__)       \
	) {                                                                                    \
		Instruction instr;                                                                 \
		instr.id                = InstrID::NAME;                                           \
		CAT(instr.instr_, NAME) = instr_structs::NAME(                                     \
			FOR_EACH_CUSTOM_LAST(HANDLE_CONS EXPAND, HANDLE_CONS_LAST EXPAND, __VA_ARGS__) \
		);                                                                                 \
		return instr;                                                                      \
	}
#include "instruction_definitions.hpp"
#undef HANDLE_ARG
#undef HANDLE_ARG_LAST
#undef HANDLE_CONS
#undef HANDLE_CONS_LAST
#undef HANDLE_INSTR_ARGS
}

#ifdef ARG_NAMESPACE_DEFAULT
	#undef ARG_NAMESPACE_DEFAULT
	#undef ARG_NAMESPACE
#endif

// NOLINTEND
