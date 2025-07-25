#pragma once

#include <base/string_id.hpp>

#include <string>

namespace compiler::driver {
	enum class BackendType { LLVM, DVM };

	std::string backendTypeToStr(BackendType type);
}
