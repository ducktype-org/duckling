#pragma once
#include <vm/core/thread/vmvalue.hpp>

namespace vm {
	using ProgramRunArguments  = std::vector<std::string>;
	using FunctionRunArguments = std::vector<Ref<vm::VmValue>>;
	using RunArguments         = std::variant<ProgramRunArguments, FunctionRunArguments>;

	struct FileCoordinates {
		usize line;
		usize column;

		auto operator<=>(const FileCoordinates&) const = default;
	};
}
