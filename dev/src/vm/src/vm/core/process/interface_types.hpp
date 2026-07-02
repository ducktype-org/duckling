#pragma once
#include <vm/core/vmvalue/ivmvalue.hpp>

namespace vm {
	using ProgramRunArguments  = std::vector<std::string>;
	using FunctionRunArguments = std::vector<Ref<vm::IVmValue>>;
	using RunArguments         = std::variant<ProgramRunArguments, FunctionRunArguments>;
}
