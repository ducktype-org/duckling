#include "component_hash.hpp"

namespace compiler::frontend {

	[[nodiscard]] std::string ComponentHash::str() const {
		std::string out;
		bool        first = true;
		for (const auto& e: elements) {
			if (!first) out.push_back('.');
			out.append(e);
			first = false;
		}
		return out;
	}
}
