// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "component_hash.hpp"

#include <base/config/build_type.hpp>

#include <algorithm>
#include <stdexcept>

namespace hashing {

	std::vector<std::string> ComponentHash::elements() const {
#if defined(BUILD_TYPE_DEV)
		std::vector<std::string> out;
		for (auto link = &last_element; link->has_value(); link = &link->value()->parent)
			out.push_back(link->value()->name);
		std::ranges::reverse(out);
		return out;
#else
		throw std::logic_error("ComponentHash::elements() is not available in Release build");
#endif
	}

	std::string ComponentHash::str() const {
		IF_BUILD_TYPE_DEV({
			std::string out;
			bool        first = true;
			for (const auto& e: elements()) {
				if (!first) out.push_back('.');
				out.append(e);
				first = false;
			}
			return out;
		})

		IF_BUILD_TYPE_RELEASE({
			// .str is not available in Release build
			throw std::logic_error("ComponentHash::str() is not available in Release build");
		})
	}

	void ComponentHash::addElementToPath([[maybe_unused]] std::string_view name) {
#if defined(BUILD_TYPE_DEV)
		last_element = base::makeSharedBox<PathElement>(std::string(name), std::move(last_element));
#endif
	}
}
