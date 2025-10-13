#pragma once

#include <base/optional.hpp>
#include <base/string_id.hpp>

#include <hashing/hash.hpp>

namespace compiler::frontend {
	/**
	 * ComponentHash - stores both the streaming (partial) hash state for a module path
	 * and the finalized hash value. The constructor computes the partial hash by
	 * optionally starting from parent's partial hash, adding the module name and
	 * finalizing to produce hash.
	 */
	struct ComponentHash {
		// Hash algorithm and result type used for module path hashing
		using HashAlg  = hashing::StatefulHash<hashing::SHA256, void>;
		using HashType = HashAlg::result_type;

		HashAlg                  partial;
		HashType                 hash;
		std::vector<std::string> elements;

		// Default constructible so ModuleTree member can be value-initialized
		constexpr ComponentHash() noexcept = default;

		// Construct from optional parent partial hash and module name
		constexpr ComponentHash(
			const base::Optional<ComponentHash>& parent, base::StrID name
		) noexcept {
			if (parent.has_value()) {
				elements = parent->elements;
				partial  = parent->partial;
			}
			if (name.isGood()) {
				elements.emplace_back(name.strView());
				// add module name to partial hash
				hashing::addToHash(partial, name);
			}

			// finalize to obtain the concrete hash value
			hash = partial.finalize();
		}

		ComponentHash(const ComponentHash& parent, std::string_view ext) noexcept:
			  partial(parent.partial),
			  elements(parent.elements) {
			if (!ext.empty()) {
				elements.emplace_back(ext);
				// add string to partial hash
				hashing::addToHash(partial, std::string_view(ext));
			}
			hash = partial.finalize();
		}

		// Construct directly from a vector of string elements
		explicit ComponentHash(const std::vector<std::string>& elems) noexcept: elements(elems) {
			// build partial by hashing all elements in order
			for (const auto& e: elements) hashing::addToHash(partial, std::string_view(e));
			hash = partial.finalize();
		}

		// Return elements joined by '.'
		[[nodiscard]] std::string str() const {
			std::string out;
			bool        first = true;
			for (const auto& e: elements) {
				if (!first) out.push_back('.');
				out.append(e);
				first = false;
			}
			return out;
		}

		// Allow hashing utilities to decompose ComponentHash by its elements
		friend constexpr auto hashDecompose(const ComponentHash& t) noexcept {
			return std::tie(t.elements);
		}
	};
}
