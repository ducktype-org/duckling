#include <driver_private/mark_nodes.hpp>
#include <frontend/module_tree/functors.hpp>
#include <frontend/module_tree/module_tree.hpp>
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
		base::Ref<std::vector<pst::LangElement::HashType>> out
	) {
		using namespace compiler::frontend;
		auto module_ref = getModuleRef(module_id);

		auto collect_from_pst = [&](auto& pst_ref) {
			auto root = pst_ref->getRootElement();
			if (auto maybe = root.illegalAccess()) {
				auto el = maybe.value();
				out->push_back(el->getHash());
			}
			auto elems = pst::viewAllSubTreeElements(root);
			for (auto& el: elems)
				if (auto maybe = el.illegalAccess()) {
					auto ptr = maybe.value();
					out->push_back(ptr->getHash());
				}
		};

		// Process main source file if present
		if (module_ref->hasMainSourceFile()) {
			auto sf = module_ref->getMainSourceFile();
			auto sf_mut
				= GetFileID_Functor::getFileRefUseOnlyWhenYouKnowWhatYouAreDoingThisCanModifyInput(
					sf->getFileID()
				);
			auto pst = sf_mut->getPST();
			collect_from_pst(pst);
		}

		// Process other source files
		for (auto& sf_ref: module_ref->getSourceFiles()) {
			// We using mutable reference for calculating the PST and hashes.
			// This is done before query-based compilation starts, so it won't break anything.
			auto sf_mut
				= GetFileID_Functor::getFileRefUseOnlyWhenYouKnowWhatYouAreDoingThisCanModifyInput(
					sf_ref->getFileID()
				);
			auto pst = sf_mut->getPST();
			collect_from_pst(pst);
		}

		// Recurse into submodules
		for (const auto& [name, submodule]: module_ref->getSubmodules())
			collectFromModule(submodule->getModuleID(), out);
	}

	std::vector<query::external::InputData> collectAllPstElementHashesFromGlobalPackages() {
		std::vector<pst::LangElement::HashType> hashes;

		const auto& packages = global_state::getPackages();
		for (const auto& pkg: packages) {
			std::filesystem::path p = pkg.package_path.getPath();
			fs::File              file(pkg.package_path);
			auto                  root_module
				= compiler::frontend::createModuleTree(file, pkg.package_name.strView());
			collectFromModule(root_module, base::Ref(&hashes));
		}

		std::vector<query::external::InputData> out;
		out.reserve(hashes.size());
		for (const auto& h: hashes) out.emplace_back(pst::internal::PSTAccessSideInput::getID(), h);

		return out;
	}

}  // namespace compiler::driver
