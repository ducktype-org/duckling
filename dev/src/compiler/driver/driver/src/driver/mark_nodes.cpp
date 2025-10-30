#include "mark_nodes.hpp"

#include <frontend/module_tree/functors.hpp>
#include <frontend/module_tree/module_tree.hpp>
#include <frontend/module_tree/source_file.hpp>
#include <frontend/pst_parser/test_utils/pst_test_utils.hpp>
#include <global_state/packages.hpp>

#include <filesystem/file.hpp>
#include <query_framework/internal/context_access.hpp>
#include <query_framework/internal/query_graph/query_graph.hpp>

namespace compiler::driver {

	// Traverse module tree recursively and collect PST element hashes.
	static void collectFromModule(
		compiler::frontend::ModuleID module_id, std::unordered_set<pst::LangElement::HashType>& out
	) {
		using namespace compiler::frontend;
		auto module_ref = getModuleRef(module_id);

		auto collect_from_pst = [&](auto& pst_ref) {
			auto root = pst_ref->getRootElement();
			if (auto maybe = root.illegalAccess()) out.insert(maybe.value()->getHash());
			auto elems = pst::viewAllSubTreeElements(root);
			for (auto& el: elems)
				if (auto maybe = el.illegalAccess()) out.insert(maybe.value()->getHash());
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

	std::unordered_set<pst::LangElement::HashType> collectAllPstElementHashesFromGlobalPackages() {
		std::unordered_set<pst::LangElement::HashType> result;

		const auto& packages = global_state::getPackages();
		for (const auto& pkg: packages) {
			std::filesystem::path p = pkg.package_path.getPath();
			fs::File              file(pkg.package_path);
			auto                  root_module
				= compiler::frontend::createModuleTree(file, pkg.package_name.strView());
			collectFromModule(root_module, result);
		}

		return result;
	}

	void markPreviousGraphNodesGreenForPstHashes() {
		using namespace query::internal;

		// Get query state
		auto state = ContextAccess::getState();

		// Attempt to fetch previous graph
		auto maybe_prev = state->getPreviousGraph();
		if (!maybe_prev.has_value()) return;  // nothing to do

		// Collect PST element hashes from global packages
		auto hashes = collectAllPstElementHashesFromGlobalPackages();

		auto prev_graph = maybe_prev.value();

		// Get all nodes from the previous graph
		auto nodes = prev_graph->getAllNodes();

		for (const auto& node: nodes) {
			// CORE_ASSERT(prev_graph->getNodeDeps(node).size() >= 1, "Node has no dependencies: ",
			// node.q_id.getData().name);
			//  @TODO: #1433 use tags
			if (node.q_id.hasStableHash() == false) {
				CORE_ASSERT(
					!hashes.contains(node.hash.val),
					"Node with unstable hash found in PST hashes: ",
					node.q_id.getData().name,
					" with hash ",
					node.hash.val.toStringHex()
				);
				continue;
			}

			// We mark only leaf nodes
			if (prev_graph->hasDependencies(node)) continue;

			// Queries in tsh have no stable hash and they are leafs so we will ignore them
			// CORE_ASSERT(node.q_id.hasStableHash(), "Leafs in previous graph must have stable
			// hashes: ", node.q_id.getData().name);

			// node.hash.val and LangElement::HashType are both base::Bit256 — compare directly
			if (hashes.contains(node.hash.val)) {
				CORE_ASSERT(
					node.q_id.getData().type == query::internal::QueryType::SideInput,
					"Now only side input should be leafs, if inputs are added feel free to remove "
				    "this assert"
				);
				state->setPrevNodeColor(node, QueryState::PrevColor::Green);
				std::cout << "Marked node as green: " << node.q_id.getData().name << '\n';
			} else {
				// Mark node as red
				state->setPrevNodeColor(node, QueryState::PrevColor::Red);
				std::cout << "Marked node as red: " << node.q_id.getData().name << '\n';
			}
		}
	}

}  // namespace compiler::driver
