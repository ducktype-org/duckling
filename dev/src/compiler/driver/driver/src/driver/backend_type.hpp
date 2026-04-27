#pragma once

#include <archiver/archive.hpp>
#include <linker/link.hpp>

#include <base/types/ints.hpp>

#include <string>

namespace compiler::driver {
	enum class BackendType : u64 { LLVM, DVM };

	std::string backendTypeToStr(BackendType type);
}
