
#include "backend_type.hpp"

#include <base/except/exceptions.hpp>

namespace compiler::driver {

	std::string backendTypeToStr(BackendType type) {
		switch (type) {
		case BackendType::LLVM:
			return "llvm";
		case BackendType::DVM:
			return "dvm";
		default:
			CORE_UNREACHABLE();
		}
	}
}
