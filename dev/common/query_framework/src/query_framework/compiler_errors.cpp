#include <iostream>
#include "query_impl.hpp"

namespace query {
	void ContextType::compilationError(std::string_view error) {
		std::cerr << "[COMPILATION ERROR]: " << error << "\n";
	}
}
