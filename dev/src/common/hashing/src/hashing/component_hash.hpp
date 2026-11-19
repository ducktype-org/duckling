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
#include <base/pointers/shared_box.hpp>

#include <string_id/string_id.hpp>

#include <algorithm>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

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
	 * In Dev builds the path elements are kept as a list linked from the last element
	 * to the root, shared with the parent path, only for testing and debugging purposes
	 * (pretty print). Each component adds one element, so the memory stays linear in the
	 * number of components. For calculating the hash of the path it is not used.
	 */
	struct ComponentHash final {
		// Hash algorithm and result type used for hierarchical path hashing
		using HashAlg  = StatefulHash<hashing::SHA256>;
		using HashType = HashAlg::result_type;

		HashAlg  partial;
		HashType hash;

		/**
		 * One element of the path, linked to the last element of its parent path.
		 */
		struct PathElement final {
			std::string                                   name;
			base::Optional<base::CSharedBox<PathElement>> parent;
		};

		using LastElementType = std::conditional_t<
			base::IS_BUILD_TYPE_DEV,
			base::Optional<base::CSharedBox<PathElement>>,
			base::Ignore>;

		[[no_unique_address]]
		LastElementType last_element;

		// Default constructible so containers holding ComponentHash can be value-initialized
		constexpr ComponentHash() noexcept = default;

		// Construct from optional parent partial hash and a component identifier
		constexpr ComponentHash(
			const base::Optional<ComponentHash>& parent, base::StrID name
		) noexcept {
			if (parent.has_value()) {
				last_element = parent->last_element;
				partial      = parent->partial;
			}
			if (name.isGood()) {
				IF_BUILD_TYPE_DEV(addElementToPath(name.strView()));

				// add component identifier to partial hash
				hashing::addToHash(partial, name);
			}

			// finalize to obtain the concrete hash value
			hash = partial.finalize();
		}

		constexpr ComponentHash(const ComponentHash& parent, std::string_view ext) noexcept:
			  partial(parent.partial),
			  last_element(parent.last_element) {
			if (!ext.empty()) {
				IF_BUILD_TYPE_DEV(addElementToPath(ext));

				// add extra fragment to partial hash
				hashing::addToHash(partial, std::string_view(ext));
			}
			hash = partial.finalize();
		}

		explicit ComponentHash(base::StrID name) noexcept: ComponentHash({}, name) {}

		// Construct directly from a vector of path elements
		explicit ComponentHash(const std::vector<std::string>& elems) noexcept {
			// build partial by hashing all elements in order
			for (const auto& e: elems) {
				IF_BUILD_TYPE_DEV(addElementToPath(e));
				hashing::addToHash(partial, std::string_view(e));
			}
			hash = partial.finalize();
		}

		/**
		 * Return the path elements in order, from the root to this component.
		 * Works only in Dev builds, throws std::logic_error otherwise.
		 * Takes an explicit object parameter, so it is a template: outside Dev the skipped
		 * branch is never instantiated, and `base::Ignore` needs no `has_value()`.
		 */
		[[nodiscard]] std::vector<std::string> elements(this const auto& self) {
			if constexpr (base::IS_BUILD_TYPE_DEV) {
				std::vector<std::string> out;
				const auto*              link = &self.last_element;
				while (link->has_value()) {
					out.push_back(link->value()->name);
					link = &link->value()->parent;
				}
				std::ranges::reverse(out);
				return out;
			} else {
				throw std::logic_error("ComponentHash::elements() is not available in Release build"
				);
			}
		}

		/**
		 * Return elements joined by '.' (represents the hierarchical path).
		 * Works only in Dev builds.
		 */
		[[nodiscard]] std::string str() const;

	private:
		/**
		 * Link a new last element with the given name to the current path.
		 * Works only in Dev builds, throws std::logic_error otherwise.
		 */
		void addElementToPath([[maybe_unused]] std::string_view name) {
			if constexpr (base::IS_BUILD_TYPE_DEV) {
				last_element
					= base::makeSharedBox<PathElement>(std::string(name), std::move(last_element));
			} else {
				throw std::logic_error(
					"ComponentHash::addElementToPath() is not available in Release build"
				);
			}
		}
	};
}
