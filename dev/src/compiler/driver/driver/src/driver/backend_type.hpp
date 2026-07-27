#pragma once

#include <base/types/ints.hpp>

#include <string>

namespace compiler::driver {
	enum class BackendType : u64 {
		LLVM,
		DVM,
		COUNT  //> Sentinel: the number of real backend types. Must stay last.
	};

	std::string backendTypeToStr(BackendType type);
}
