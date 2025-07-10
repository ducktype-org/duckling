#pragma once

#include <base/string_id.hpp>

#include <string>

namespace compiler::driver {
	enum class BackendType : bool { LLVM, DVM };

	std::string backendTypeToStr(BackendType type);
}
