#include "collect_input.hpp"

#include <frontend/module_tree/functors.hpp>
#include <frontend/module_tree/module_tree.hpp>
#include <frontend/module_tree/queries.hpp>
#include <frontend/module_tree/source_file.hpp>
#include <frontend/pst_parser/pst_query/pst_access_side_input.hpp>
#include <frontend/pst_parser/test_utils/pst_test_utils.hpp>
#include <global_state/packages.hpp>

#include <base/collections/stable_hashmap.hpp>
#include <base/except/exceptions.hpp>
#include <base/pointers/ref.hpp>

#include <filesystem/file.hpp>
#include <hashing/add_to_hash.hpp>
#include <query_framework/external/api.hpp>
#include <query_framework/internal/query_data/query_id.hpp>

#include <vector>

namespace compiler::driver {

	namespace {
		/**
		 * @brief Structure to hold module child lookup information.
		 * Lookups look for a child with a specific name.
		 * And that submodule may or may not be found.
		 * Also stores InputData for efficient reuse without recalculating hash.
		 */
		struct LookupValue final {
			base::StrID                child_name;
			bool                       found;
			query::external::InputData input_data;
		};

		// Map from parent module hash to list of child lookups
		// The key is the hash of the parent module, because that's what QueryModuleChildSideInput
		// depends on, and we want to quickly find all lookups related to a module when we process
		// it during input collection.
		using LookupsMap
			= base::StableHashMap<hashing::ComponentHash::HashType, std::vector<LookupValue>>;

		/**
		 * @brief Create a lookups map from the provided module lookup metadata.
		 * This map will be used during input collection to find all lookups related to a module and
		 * check if they are still valid.
		 * @param module_lookups Vector of MetadataInfo for module lookups, containing the metadata
		 * for all QueryModuleChildSideInput queries.
		 * @return LookupsMap mapping parent module hash to list of child lookups.
		 */
		LookupsMap createLookupMap(
			const std::vector<query::external::MetadataInfo<frontend::metadata_ModuleLookup>>&
				module_lookups
		) {
			LookupsMap lookups_map;

			for (const auto& lookup: module_lookups) {
				// Skip entries that don't belong to QueryModuleChildSideInput
				// (metadata may be attached to other query types as well)
				if (lookup.input_data.q_id != frontend::QueryModuleChildSideInput::getID())
					continue;

				auto key = lookup.value->value.parent_hash;
				if (!lookups_map.contains(key)) lookups_map.put(key, std::vector<LookupValue>{});
				lookups_map.atMaybe(key).value()->emplace_back(LookupValue{
					.child_name = lookup.value->value.child_name,
					.found      = lookup.value->value.found,
					.input_data = lookup.input_data,
				});
			}
			return lookups_map;
		}
	}

	void collectQueryInputsFromPst(
		CRef<pst::ParsedPST<>> pst_ref, std::vector<query::external::InputData>& out
	) {
		auto root = pst_ref->getRootElement();
		if (auto maybe_root = root.illegalAccess()) {
			auto root_unlocked = maybe_root.value();
			out.emplace_back(pst::internal::PSTAccessSideInput::getID(), root_unlocked->getHash());
		}

		auto elems = pst::viewAllSubTreeElements(root);
		for (auto& el: elems)
			if (auto maybe_elem = el.illegalAccess()) {
				auto ptr = maybe_elem.value();
				out.emplace_back(pst::internal::PSTAccessSideInput::getID(), ptr->getHash());
			}
	}

	/**
	 * @brief Collect all input data (QueryID + hash) from the global packages.
	 * This includes:
	 * - Module side inputs for all modules in the package.
	 * - File side inputs for all source files in the package.
	 * - PST access side inputs for all PST elements in the package.
	 * - Source file count and submodule count side inputsW for all modules.
	 * - Module child side inputs for all module lookups performed in the previous compilation, that
	 * 	 are still valid in the current module tree.
	 */
	static void collectFromModule(
		const LookupsMap&                                  lookups_map,
		compiler::frontend::ModuleID                       module_id,
		base::Ref<std::vector<query::external::InputData>> out
	) {
		using namespace compiler::frontend;
		auto module_ref = getModuleRef(module_id);

		// Collect module side input
		out->emplace_back(QueryModuleSideInput::getID(), ModuleTree::getModuleHash(module_id));

		auto submodules_count = module_ref->getSubmodules().illegalAccess().size();
		out->emplace_back(
			QuerySubmoduleCountSideInput::getID(),
			KeyOf_SubmoduleCountSideInput::computeHash(module_id, submodules_count)
				.queryStablePerfectHash()
		);

		// Collect ModuleChildSideInput for all lookups done on this module
		auto module_hash = ModuleTree::getModuleHash(module_id);
		auto lookup_res  = lookups_map.atMaybe(module_hash);
		if (lookup_res.has_value()) {
			for (const auto& lookup: *lookup_res.value()) {
				// Check if the lookup is still valid
				auto child_ref = module_ref->getSubmoduleByName(lookup.child_name).illegalAccess();
				bool found     = child_ref.has_value();

				// If the lookup result is the same as before, we can reuse the stored InputData
				// No need to recalculate the hash since it's already stored in input_data
				if (found == lookup.found) out->emplace_back(lookup.input_data);
			}
		}

		// Process main source file if present
		if (module_ref->hasMainSourceFile()) {
			auto sf = module_ref->getMainSourceFile();

			// We using mutable reference for calculating the PST and hashes.
			// This is done before query-based compilation starts, so it won't break anything.
			auto sf_mut
				= GetFileID_Functor::getFileRefUseOnlyWhenYouKnowWhatYouAreDoingThisCanModifyInput(
					sf.illegalAccess().getID()
				);

			// Collect file side input
			out->emplace_back(QueryFileSideInput::getID(), sf_mut->getComponentHash().hash);

			auto pst = sf_mut->getPST();
			collectQueryInputsFromPst(pst, *out);
		}

		// Recurse into submodules
		for (const auto& submodule: module_ref->getSubmodules().illegalAccess())
			collectFromModule(lookups_map, submodule.illegalAccess().getID(), out);
	}

	namespace {
		std::vector<query::external::InputData> collectInputDataFromGlobalPackagesImpl(
			const LookupsMap& lookups_map
		) {
			std::vector<query::external::InputData> out;

			for (const auto& package: global_state::getPackages())
				collectFromModule(
					lookups_map, package.getRootModule().illegalAccess().getID(), base::Ref(&out)
				);

			return out;
		}
	}

	std::vector<query::external::InputData> collectInputDataFromGlobalPackagesFromPrevMetadata() {
		// This function should only be called after loading previous graph and metadata.
		CORE_ASSERT(
			query::external::prevMetadataExists(),
			"Previous metadata must be loaded before collecting input data. This indicates a bug "
			"in driver initialization."
		);

		// Collect all metadata_ModuleLookup to recreate ModuleChildSideInput nodes.
		// If a child that was looked up is in the same state as in the previous compilation
		// (i.e., the parent module still has the same child with the same name),
		// then the ModuleChildSideInput node can be marked as unchanged.
		// Similarly, if the module still doesn't have such a child, the node can be marked as
		// unchanged too. Therefore, we need to check all such lookups and create inputs for them if
		// still valid.

		// This map could technically be created only for "not found" lookups because if a child
		// exists, we can just add the corresponding input by looking at the current ModuleTree.
		// However, in practice it's easier to check all lookups. This should not be a performance
		// issue because the number of lookups is expected to be low, but this can be changed if
		// needed.
		LookupsMap lookups_map = createLookupMap(
			query::external::getMetadataFromAllPrevNodes<frontend::metadata_ModuleLookup>()
		);

		return collectInputDataFromGlobalPackagesImpl(lookups_map);
	}

	std::vector<query::external::InputData> collectInputDataFromGlobalPackagesFromCurrentMetadata() {
		LookupsMap lookups_map = createLookupMap(
			query::external::getMetadataFromAllCurrentNodes<frontend::metadata_ModuleLookup>()
		);

		return collectInputDataFromGlobalPackagesImpl(lookups_map);
	}

}  // namespace compiler::driver
