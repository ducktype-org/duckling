#pragma once

#include <frontend/module_tree/module_id.hpp>
#include <frontend/pst_parser/access.hpp>
#include <frontend/pst_parser/lang_parser_element.hpp>
#include <helios/scope_id.hpp>

#include <base/collections/optional.hpp>
#include <base/extend_cpp/strongly_typed_id.hpp>

namespace compiler::helios {

	STRONG_TYPEDEF_ID(ScopeInternalID);

	/**
	 * Structure holding all data directly stored for each created scope.
	 * @note This structure is only used to store data inside query cache,
	 * and should not be used directly outside of it.
	 * The main way to interact with scopes is through ScopeID and related functions/queries.
	 */
	struct ScopeData final {
		base::Optional<ScopeID> parent;

		bool is_root;

		/**
		 * @brief PST element for which the scope was created.
		 * Empty for root scope.
		 */
		base::Optional<pst::HashType> related_pst_element_hash;

		/**
		 * @brief Module, the scope was defined in
		 */
		frontend::ModuleID parent_module;

		/**
		 * Scope depth, i.e. distance to the root scope.
		 * It is currently unused but might be useful in the future.
		 */
		u64 depth;

		/**
		 * @brief ID used for unstable perfect hashing of scopes.
		 */
		ScopeInternalID unstable_id;

		ScopeData(
			base::Optional<ScopeID>       parent,
			bool                          is_root,
			base::Optional<pst::HashType> related_pst_element_hash,
			frontend::ModuleID            parent_module,
			u64                           depth
		):
			  parent(parent),
			  is_root(is_root),
			  related_pst_element_hash(related_pst_element_hash),
			  parent_module(parent_module),
			  depth(depth),
			  unstable_id(ScopeInternalID::next()) {
			CORE_ASSERT(
				related_pst_element_hash.empty() == is_root,
				"Non-root scope must have related pst element."
			);
		}

		[[nodiscard]] base::Optional<pst::AccessLocked<pst::LangElement>> relatedPSTElement() const;

		/**
		 * @brief Creates a perfect clone of this ScopeData,
		 * with all data copied as-is, including unstable_id.
		 *
		 * @note It is used by the QueryPrimaryCodeScopeFor query
		 * when the result is effectively the same as the parent scope,
		 * but a new ScopeData object is still needed.
		 */
		[[nodiscard]]
		ScopeData perfectClone() const;

		ScopeData(ScopeData&&)                 = default;
		ScopeData(const ScopeData&)            = delete;
		ScopeData& operator=(const ScopeData&) = delete;
	};

}
