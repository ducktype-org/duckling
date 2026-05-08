#pragma once

#include <base/types/ints.hpp>

namespace vm::fast {
	enum class InstrID : u64 {
#define HANDLE_INSTR(NAME) NAME,
#include "instruction_definitions.hpp"
#undef HANDLE_INSTR
	};
}
