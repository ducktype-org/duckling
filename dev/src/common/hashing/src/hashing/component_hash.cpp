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

	namespace {
		/**
		 * Walk the path from its last element to the root and return the names root first.
		 * A template, so that it is only instantiated where `last_element` is a real list (Dev).
		 */
		template<typename LastElement>
		std::vector<std::string> collectPathElements(const LastElement& last_element) {
			std::vector<std::string> out;
			for (auto link = &last_element; link->has_value(); link = &link->value()->parent)
				out.push_back(link->value()->name);
			std::ranges::reverse(out);
			return out;
		}
	}

	std::vector<std::string> ComponentHash::elements() const {
		if constexpr (base::IS_BUILD_TYPE_DEV)
			return collectPathElements(last_element);
		else
			throw std::logic_error("ComponentHash::elements() is not available in Release build");
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
		if constexpr (base::IS_BUILD_TYPE_DEV) {
			last_element
				= base::makeSharedBox<PathElement>(std::string(name), std::move(last_element));
		} else {
			throw std::logic_error(
				"ComponentHash::addElementToPath() is not available in Release build"
			);
		}
	}
}
