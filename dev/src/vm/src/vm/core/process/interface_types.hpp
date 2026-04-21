#pragma once
#include <vm/core/vmvalue/vmvalue.hpp>

namespace vm {
	using ProgramRunArguments  = std::vector<std::string>;
	using FunctionRunArguments = std::vector<Ref<vm::VmValue>>;
	using RunArguments         = std::variant<ProgramRunArguments, FunctionRunArguments>;
}
