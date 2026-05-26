#pragma once

#include <vm/core/safe/low_program/instruction.hpp>

namespace vm::jit::helpers {
	void trampoline(OPFUN_REF_ARGS);
}
