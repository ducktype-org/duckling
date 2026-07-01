#pragma once

#include <base/extend_cpp/strongly_typed_id.hpp>

namespace vm::fast {
	STRONG_TYPEDEF_ID_DIRECT_CREATION(FunctionID);
	STRONG_TYPEDEF_ID_DIRECT_CREATION(GlobalDataID);
	STRONG_TYPEDEF_ID_DIRECT_CREATION(TypeID);

	/**
	 * @brief Enum of all instruction IDs.
	 */
	enum class InstrID : u64 {
#define HANDLE_INSTR(NAME) NAME,
#include "instructions/instruction_definitions.hpp"
#undef HANDLE_INSTR
	};
}
