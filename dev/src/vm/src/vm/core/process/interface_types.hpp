#pragma once

#include <filesystem/file.hpp>
#include <vm/core/thread/vmvalue.hpp>

namespace vm {
	using ProgramRunArguments  = std::vector<std::string>;
	using FunctionRunArguments = std::vector<Ref<vm::VmValue>>;
	using RunArguments         = std::variant<ProgramRunArguments, FunctionRunArguments>;

	struct FatBytecodePosition {
		fs::File file;
		usize    line;
		usize    column;
	};
}
