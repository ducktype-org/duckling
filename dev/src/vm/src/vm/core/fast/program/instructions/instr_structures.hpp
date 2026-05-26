/**
 * @brief This file define the instruction structures and instruction union
 * given argument type definitions. It also defines instruction maker functions for easier
 * instruction creation.
 * You need to include "ids.hpp" before this file, which defines the
 * instruction IDs, and define the argument types and verify them with "argument_definitions.hpp".
 * Example usages are in "relocatable.hpp" and "executable.hpp".
 */
// NOLINTBEGIN

#ifndef ARG_NAMESPACE
	#define ARG_NAMESPACE_DEFAULT
	#define ARG_NAMESPACE
#endif

#ifndef ID_TYPE
	#define ID_TYPE_DEFAULT
	#define ID_TYPE()               InstrID
	#define MAKE_ID_FROM_NAME(NAME) InstrID::NAME
#endif


#ifdef MAKE_INSTR_STRUCTS
namespace instr_structs {
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
#endif

#ifdef MAKE_INSTRUCTION_UNION
struct Instruction {
	union {
	#define HANDLE_INSTR(NAME) instr_structs::NAME CAT(instr_, NAME);
	#include "instruction_definitions.hpp"
	#undef HANDLE_INSTR
	};

	ID_TYPE() id;
};
#endif

#if defined(MAKE_MAKERS_JUST_DEF) || defined(MAKE_MAKERS_FULL) || defined(MAKE_MAKERS_JUST_IMPL)
namespace maker {
	#define HANDLE_ARG(type, name)       ARG_NAMESPACE type name,
	#define HANDLE_ARG_LAST(type, name)  ARG_NAMESPACE type name
	#define HANDLE_CONS(type, name)      .name = std::move(name),
	#define HANDLE_CONS_LAST(type, name) .name = std::move(name)
	#define MAKER_SIGNATURE(NAME, ...)                                                   \
		Instruction NAME(                                                                \
			FOR_EACH_CUSTOM_LAST(HANDLE_ARG EXPAND, HANDLE_ARG_LAST EXPAND, __VA_ARGS__) \
		)
	#define MAKER_BODY(NAME, ...)                                               \
		{                                                                       \
			Instruction instr{                                                  \
				.id                = MAKE_ID_FROM_NAME(NAME),                   \
				.CAT(instr_, NAME) = instr_structs::NAME{ FOR_EACH_CUSTOM_LAST( \
					HANDLE_CONS EXPAND, HANDLE_CONS_LAST EXPAND, __VA_ARGS__    \
				) },                                                            \
			};                                                                  \
			return instr;                                                       \
		}
	#ifdef MAKE_MAKERS_FULL
		#define HANDLE_INSTR_ARGS(NAME, ...) \
			constexpr MAKER_SIGNATURE(NAME, __VA_ARGS__) MAKER_BODY(NAME, __VA_ARGS__)
	#elif defined(MAKE_MAKERS_JUST_IMPL)
		#define HANDLE_INSTR_ARGS(NAME, ...) \
			MAKER_SIGNATURE(NAME, __VA_ARGS__) MAKER_BODY(NAME, __VA_ARGS__)
	#elif defined(MAKE_MAKERS_JUST_DEF)
		#define HANDLE_INSTR_ARGS(NAME, ...) MAKER_SIGNATURE(NAME, __VA_ARGS__);
	#endif
	#include "instruction_definitions.hpp"
	#undef HANDLE_ARG
	#undef HANDLE_ARG_LAST
	#undef HANDLE_CONS
	#undef HANDLE_CONS_LAST
	#undef HANDLE_INSTR_ARGS
}
#endif

#ifdef ARG_NAMESPACE_DEFAULT
	#undef ARG_NAMESPACE_DEFAULT
	#undef ARG_NAMESPACE
#endif

#ifdef ID_TYPE_DEFAULT
	#undef ID_TYPE_DEFAULT
	#undef ID_TYPE
	#undef MAKE_ID_FROM_NAME
#endif

// NOLINTEND
