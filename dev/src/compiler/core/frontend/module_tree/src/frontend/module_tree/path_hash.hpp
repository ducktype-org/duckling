#pragma once

#include <hashing/hash.hpp>
#include <base/string_id.hpp>
#include <base/optional.hpp>

namespace compiler::frontend {
	/**
	 * PathHash - stores both the streaming (partial) hash state for a module path
	 * and the finalized hash value. The constructor computes the partial hash by
	 * optionally starting from parent's partial hash, adding the module name and
	 * finalizing to produce hash.
	 */
	struct PathHash {
		// Hash algorithm and result type used for module path hashing
        using HashAlg  = hashing::StatefulHash<hashing::SHA256, void>;
        using HashType = HashAlg::result_type;

		HashAlg partial;
		HashType hash;
        std::vector<std::string> elements;

		// Default constructible so ModuleTree member can be value-initialized
		constexpr PathHash() noexcept = default;

		// Construct from optional parent partial hash and module name
		constexpr PathHash(const base::Optional<PathHash>& parent, base::StrID name) noexcept{
            if (parent.has_value()){
                elements = parent->elements;
                partial  = parent->partial;
            }
            if(name.isGood()){
                elements.emplace_back(name.strView());
                // add module name to partial hash
                hashing::addToHash(partial, name);
            }

            // finalize to obtain the concrete hash value
            hash = partial.finalize();
        }
	};
}