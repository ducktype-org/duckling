#pragma once

#include <string_id/string_id.hpp>

namespace vm::loader {
	struct FatBytecodePosition {
		base::StrID function_name;
		usize       instruction_index;
	};

	enum class MappingException {
		MissingMapping,
		NoFunction,
	};
}