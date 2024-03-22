#include <iostream>
#include "query_impl.hpp"

namespace query {

	void detail::ContextType::compilationError(std::string_view error) {
		std::cerr << "[COMPILATION ERROR]: " << error << "\n";
	}

}

