// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include "hash.hpp"

#include <base/collections/optional.hpp>
#include <base/config/build_type.hpp>
#include <base/misc/ignore.hpp>

#include <string_id/string_id.hpp>

#include <type_traits>

namespace hashing {
	/**
	 * ComponentHash - stores both the streaming (partial) hash state for a
	 * hierarchical component path and the finalized hash value. The constructor
	 * computes the partial hash by optionally starting from a parent's partial
	 * state, adding an identifier for this component, and finalizing to produce
	 * the concrete hash.
	 * This is useful for creating a hash of pathes like module/submodule/sourcefile
	 * where each component is identified by a base::StrID.
	 * This object can be copied and still calculatuion of path hash will be linear
	 * the vector of strings stores the path elements in order
	 * this vector is only for testing and debuging purposes (pretty print)
	 * for calculating the hash of the path it is not used
	 */
	struct ComponentHash final {
		// Hash algorithm and result type used for hierarchical path hashing
		using HashAlg  = StatefulHash<hashing::SHA256>;
		using HashType = HashAlg::result_type;

		HashAlg  partial;
		HashType hash;

		using ElementsType
			= std::conditional_t<base::IS_BUILD_TYPE_DEV, std::vector<std::string>, base::Ignore>;

		[[no_unique_address]]
		ElementsType elements;

		// Default constructible so containers holding ComponentHash can be value-initialized
		constexpr ComponentHash() noexcept = default;

		// Construct from optional parent partial hash and a component identifier
		constexpr ComponentHash(
			const base::Optional<ComponentHash>& parent, base::StrID name
		) noexcept {
			if (parent.has_value()) {
				elements = parent->elements;
				partial  = parent->partial;
			}
			if (name.isGood()) {
				IF_BUILD_TYPE_DEV(elements.emplace_back(name.strView()));

				// add component identifier to partial hash
				hashing::addToHash(partial, name);
			}

			// finalize to obtain the concrete hash value
			hash = partial.finalize();
		}

		constexpr ComponentHash(const ComponentHash& parent, std::string_view ext) noexcept:
			  partial(parent.partial),
			  elements(parent.elements) {
			if (!ext.empty()) {
				IF_BUILD_TYPE_DEV(elements.emplace_back(ext));

				// add extra fragment to partial hash
				hashing::addToHash(partial, std::string_view(ext));
			}
			hash = partial.finalize();
		}

		explicit ComponentHash(base::StrID name) noexcept: ComponentHash({}, name) {}

		// Construct directly from a vector of path elements
		explicit ComponentHash(const std::vector<std::string>& elems) noexcept: elements(elems) {
			// build partial by hashing all elements in order
			for (const auto& e: elems) hashing::addToHash(partial, std::string_view(e));
			hash = partial.finalize();
		}

		/**
		 * Return elements joined by '.' (represents the hierarchical path).
		 * Works only in Dev builds.
		 */
		[[nodiscard]] std::string str() const;
	};
}
