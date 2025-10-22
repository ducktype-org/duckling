#include "component_hash.hpp"
#include <base/config/build_type.hpp>

namespace compiler::frontend {

	std::string ComponentHash::str() const {
		IF_BUILD_TYPE_DEV({
			std::string out;
			bool        first = true;
			for (const auto& e: elements) {
				if (!first) out.push_back('.');
				out.append(e);
				first = false;
			}
			return out;
		})

		IF_BUILD_TYPE_RELEASE({
			// .str is not available in Release build
			std::terminate();
		})
	}
}
