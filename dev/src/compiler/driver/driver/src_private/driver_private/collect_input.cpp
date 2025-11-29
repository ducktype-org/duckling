#include <driver_private/collect_input.hpp>
#include <frontend/module_tree/functors.hpp>
#include <frontend/module_tree/module_tree.hpp>
#include <frontend/module_tree/queries.hpp>
#include <frontend/module_tree/source_file.hpp>
#include <frontend/pst_parser/pst_query/pst_access_side_input.hpp>
#include <frontend/pst_parser/test_utils/pst_test_utils.hpp>
#include <global_state/packages.hpp>

#include <base/pointers/ref.hpp>

#include <filesystem/file.hpp>
#include <query_framework/external/api.hpp>
#include <query_framework/internal/query_data/query_id.hpp>

#include <vector>

namespace compiler::driver {

	// Traverse module tree recursively and collect PST element hashes.
	static void collectFromModule(
		compiler::frontend::ModuleID                       module_id,
		base::Ref<std::vector<query::external::InputData>> out
	) {
		using namespace compiler::frontend;
		auto module_ref = getModuleRef(module_id);

		// Collect module side input
		out->emplace_back(
			QueryModuleSideInput::getID(), ModuleTree::getComponentHash(module_id).hash
		);

		auto collect_from_pst = [&](auto& pst_ref) {
			auto root = pst_ref->getRootElement();
			if (auto maybe = root.illegalAccess()) {
				auto el = maybe.value();
				out->emplace_back(pst::internal::PSTAccessSideInput::getID(), el->getHash());
			}
			auto elems = pst::viewAllSubTreeElements(root);
			for (auto& el: elems)
				if (auto maybe = el.illegalAccess()) {
					auto ptr = maybe.value();
					out->emplace_back(pst::internal::PSTAccessSideInput::getID(), ptr->getHash());
				}
		};

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
			collect_from_pst(pst);
		}

		// Process other source files
		for (auto& sf_ref: module_ref->getSourceFiles()) {
			// We using mutable reference for calculating the PST and hashes.
			// This is done before query-based compilation starts, so it won't break anything.
			auto sf_mut
				= GetFileID_Functor::getFileRefUseOnlyWhenYouKnowWhatYouAreDoingThisCanModifyInput(
					sf_ref.illegalAccess().getID()
				);

			// Collect file side input
			out->emplace_back(QueryFileSideInput::getID(), sf_mut->getComponentHash().hash);

			auto pst = sf_mut->getPST();
			collect_from_pst(pst);
		}

		// Recurse into submodules
		for (const auto& [name, submodule]: module_ref->getSubmodules())
			collectFromModule(submodule.illegalAccess().getID(), out);
	}

	std::vector<query::external::InputData> collectAllPstElementHashesFromGlobalPackages() {
		std::vector<query::external::InputData> out;

		const auto& packages = global_state::getPackages();
		for (const auto& pkg: packages) collectFromModule(pkg.root_module, base::Ref(&out));

		return out;
	}

}  // namespace compiler::driver
