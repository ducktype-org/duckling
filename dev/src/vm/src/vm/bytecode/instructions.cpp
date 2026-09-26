/**
 * @file instructions.cpp
 * @brief Out-of-line per-instruction parts of `Instruction`. These expand
 * `instruction_definitions.def.hpp` for all instructions, which is expensive —
 * defining them here means the cost is paid once instead of in every TU that
 * includes `instructions.hpp`.
 */
#include <vm/bytecode/instructions.hpp>

#include <array>
#include <utility>

namespace vm::code {

	// Sanity check for better errors.
#define HANDLE_INSTR(name) static_assert(internal::VeryTrivial<VM_INSTR_FROM_NAME(name)>);
#include "instruction_definitions.def.hpp"
#undef HANDLE_INSTR
	static_assert(internal::VeryTrivial<instructions::Comment>);

	base::StrID Instruction::name() const {
		static std::array<base::StrID, INSTR_COUNT> map = {
#define HANDLE_INSTR(name) base::StrID(#name),
#include "instruction_definitions.def.hpp"
#undef HANDLE_INSTR
			base::StrID("[comment]")
		};
		return map.at(std::to_underlying(opcode()));
	}

#define ARG_NAME(type, name)                 name
#define ARG_ALIAS(instr_name, arg_type_name) &alts.op_##instr_name.ARG_NAME arg_type_name,

	std::vector<opargs::OpCodeArgCRef> Instruction::args() const {
		switch (opcode()) {
#define HANDLE_INSTR_ARGS(name, ...)                           \
	case VM_OPCODE_FROM_NAME(name):                            \
		return { FOR_EACH_ARG(ARG_ALIAS, name, __VA_ARGS__) }; \
		break;
#include "instruction_definitions.def.hpp"
#undef HANDLE_INSTR_ARGS
		case OpCode::Comment:
			return {};
			break;
		}
		CORE_UNREACHABLE();
	}

#undef ARG_NAME
#undef ARG_ALIAS

	bool operator==(const Instruction& a, const Instruction& b) {
		if (a.opcode() != b.opcode()) return false;

		switch (a.opcode()) {
#define HANDLE_INSTR(name)          \
	case VM_OPCODE_FROM_NAME(name): \
		return a.get<VM_INSTR_FROM_NAME(name)>() == b.get<VM_INSTR_FROM_NAME(name)>();
			break;
#include "instruction_definitions.def.hpp"
#undef HANDLE_INSTR
		case OpCode::Comment:
			return a.get<instructions::Comment>() == b.get<instructions::Comment>();
			break;
		}
		CORE_UNREACHABLE();
	}
}
