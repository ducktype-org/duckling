#include "component_hash.hpp"

namespace compiler::frontend {


	[[nodiscard]] std::string ComponentHash::str() const {
// if-constexpr can't be used here because of C++ rules about if-constexpr errors.
// Relevant reddit discussion:
// https://www.reddit.com/r/cpp/comments/139b5wt/code_in_ifconstexpr_branch_not_taken_causing/
#ifdef BUILD_TYPE_DEV
		std::string out;
		bool        first = true;
		for (const auto& e: elements) {
			if (!first) out.push_back('.');
			out.append(e);
			first = false;
		}
		return out;
#else
		// .str is not available in Release build
		std::terminate();
#endif
	}
}
