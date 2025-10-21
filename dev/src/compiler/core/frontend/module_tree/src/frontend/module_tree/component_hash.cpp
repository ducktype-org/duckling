#include "component_hash.hpp"

namespace compiler::frontend {

	[[nodiscard]] std::string ComponentHash::str() const {
		if constexpr (base::IS_BUILD_TYPE_DEV) {
			std::string out;
			bool        first = true;
			for (const auto& e: elements) {
				if (!first) out.push_back('.');
				out.append(e);
				first = false;
			}
			return out;
		}
		else {
			// .str is not available in Release build
			std::terminate();
		}
	}
}
