#define HANDLE_ARG(type, name) type name;

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
