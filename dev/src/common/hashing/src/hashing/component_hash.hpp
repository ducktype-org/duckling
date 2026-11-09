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

#include <array>
#include <charconv>
#include <string_view>
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

	/**
	 * PathHash - a hierarchical path hash like ComponentHash, but keeping only the streaming
	 * (partial) hash state. The final hash is computed on demand (`finalize`), not for every path
	 * component. Extending a path hashes exactly the same bytes as ComponentHash, so
	 * `PathHash(ComponentHash(p, "a"))` and `PathHash(PathHash(p), "a")` have the same partial state.
	 *
	 * Used for PST element paths, where the partial state is extended for every element and only
	 * the partial state is used (as the prefix of element hashes).
	 */
	struct PathHash final {
		using HashAlg  = ComponentHash::HashAlg;
		using HashType = ComponentHash::HashType;

		/**
		 * @brief An index path component, hashed as the string "[index]".
		 */
		struct Index final {
			usize index;
		};

		HashAlg partial;

		[[no_unique_address]]
		ComponentHash::ElementsType elements;

		PathHash() noexcept = default;

		// NOLINTNEXTLINE(google-explicit-constructor)
		PathHash(const ComponentHash& component) noexcept:
			  partial(component.partial),
			  elements(component.elements) {}

		PathHash(const PathHash& parent, std::string_view ext) noexcept:
			  partial(parent.partial),
			  elements(parent.elements) {
			if (!ext.empty()) {
				IF_BUILD_TYPE_DEV(elements.emplace_back(ext));
				hashing::addToHash(partial, ext);
			}
		}

		PathHash(const PathHash& parent, Index index) noexcept:
			  partial(parent.partial),
			  elements(parent.elements) {
			// "[index]" without a heap allocation
			std::array<char, 24> buffer{};
			buffer[0] = '[';
			auto result
				= std::to_chars(buffer.data() + 1, buffer.data() + buffer.size() - 1, index.index);
			*result.ptr = ']';
			auto ext    = std::string_view(buffer.data(), result.ptr + 1);
			IF_BUILD_TYPE_DEV(elements.emplace_back(ext));
			hashing::addToHash(partial, ext);
		}

		PathHash(const PathHash&)            = default;
		PathHash(PathHash&&)                 = default;
		PathHash& operator=(const PathHash&) = default;
		PathHash& operator=(PathHash&&)      = default;

		/**
		 * @brief The hash of the whole path (same as ComponentHash::hash of the same path).
		 */
		[[nodiscard]]
		HashType finalize() const {
			HashAlg copy = partial;
			return copy.finalize();
		}

		/**
		 * Return elements joined by '.' (represents the hierarchical path).
		 * Works only in Dev builds.
		 */
		[[nodiscard]] std::string str() const;
	};
}
