#include <driver/exit.hpp>
#include <driver/initialize.hpp>
#include <driver/module_flags/module_flags.hpp>
#include <driver/operations/generic_operations.hpp>
#include <frontend/module_tree/functors.hpp>
#include <frontend/module_tree/module_tree.hpp>
#include <frontend/module_tree/queries.hpp>
#include <global_state/packages.hpp>
#include <helios/queries.hpp>

#include <artifacts/artifacts.hpp>
#include <filesystem/file_path.hpp>
#include <query_framework/external/api.hpp>
#include <query_framework/internal/context_access.hpp>
#include <query_framework/internal/query_graph/query_graph.hpp>
#include <query_framework/query_entry_point.hpp>
#include <query_framework/utils/with_context_do.hpp>
#include <string_id/string_id.hpp>
#include <tester/tester.hpp>

#include <array>
#include <filesystem>
#include <iostream>

namespace {
	std::string package_name = "driver_test_package";
}

class DriverTest final: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS DriverTest

	fs::FilePath artifacts_path = fs::FileManager::createRandomTempDirectory().getFilePath();

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		// note: all of those tests have to work on different
		// modules, since otherwise query will cache the results, and tests
		// wont test what they are supposed to:
		TESTER_ADD_TEST(graphConsistencyAfterOptimizationTest);
		TESTER_ADD_TEST(objFileGenerated);
		TESTER_ADD_TEST(assemblyAndLLVMGenerated);
		TESTER_ADD_TEST(dvmBackendRuns);
		TESTER_ADD_TEST(packageCompiles);
		TESTER_ADD_TEST(globalsTest);
		TESTER_ADD_TEST(globalsInitializationTest);
		TESTER_ADD_TEST(saveArtifactsTest);
		TESTER_ADD_TEST(sideInputsTest);

		compiler::driver::initializeTheCompiler(
			compiler::driver::CompilerModeOfOperationAndOptions::PackageCompilationMode{
				.main_package_info = {
					.package_name = package_name,
					.package_path = fs::FilePath(path("modules/functions_1")),
				},
				.compilation_artifacts = {
					.artifacts_path = artifacts_path,
				},
				.debug_options         = {},
				.incremental           = {}
			}
		);
	}

private:
	/// @brief The main graph consistency test after serialization/optimization
	void graphConsistencyAfterOptimizationTest() {
		using namespace compiler;

		// ================================================================================
		// Helper lambdas for this test
		// ================================================================================

		// Collects (all) stable dependencies of NodeID
		auto collect_stable_dependencies
			= [](const query::internal::QueryGraph& graph,
		         const query::internal::NodeID&     node) -> std::vector<query::internal::NodeID> {
			std::vector<query::internal::NodeID> result;

			const auto& deps = graph.getNodeDeps(node);
			for (const auto& dep: deps)
				if (dep.q_id.getData().usesStableHashing()) result.push_back(dep);
			return result;
		};

		// Build parent map: for each node, list nodes that have it as a child (dependency)
		auto build_parent_map
			= [](const query::internal::QueryGraph& graph
		      ) -> base::HashMap<query::internal::NodeID, std::vector<query::internal::NodeID>> {
			base::HashMap<query::internal::NodeID, std::vector<query::internal::NodeID>> parents;

			for (const auto& node: graph.getAllNodes())
				parents.emplace(node, std::vector<query::internal::NodeID>{});

			for (const auto& node: graph.getAllNodes()) {
				for (const auto& child: graph.getDirectDependencies(node)) {
					auto it = parents.find(child);
					CORE_ASSERT(it != parents.end(), "Node not it the map, graph is inconsistent");
					it->second.push_back(node);
				}
			}

			return parents;
		};

		// Compare if two vectors have the same NodeIDs inside
		auto are_node_vectors_same = [](
										 std::vector<query::internal::NodeID>& first,
										 std::vector<query::internal::NodeID>& second
									 ) -> bool {
			std::ranges::sort(first, [](auto& l, auto& r) { return l < r; });
			std::ranges::sort(second, [](auto& l, auto& r) { return l < r; });
			return first == second;
		};

		// ================================================================================
		// Sanity check: this test must run before any other tests
		// ================================================================================
		auto ctx_state          = query::internal::ContextAccess::getState();
		auto initial_graph_view = ctx_state->getGraphMutable();
		assertTrue(
			initial_graph_view->getAllNodes().empty(),
			"graphConsistencyAfterOptimizationTest must run before other driver tests to keep the"
			" compilation graph clean"
		);

		// ================================================================================
		// Precompile all modules used across other driver tests with unique package names
		// This is done to test the graph optimization as good as possible
		// ================================================================================
		struct PrecompileInfo {
			const char* module_path;
			const char* package_prefix;
		};

		constexpr std::array PRECOMPILE_MODULES{
			PrecompileInfo{ .module_path    = "modules/functions_1",
			                .package_prefix = "graph_consistency_functions_1" },
			PrecompileInfo{ .module_path    = "modules/functions_2",
			                .package_prefix = "graph_consistency_functions_2" },
			PrecompileInfo{ .module_path    = "modules/functions_3",
			                .package_prefix = "graph_consistency_functions_3" },
			PrecompileInfo{ .module_path    = "modules/functions_4",
			                .package_prefix = "graph_consistency_functions_4" },
			PrecompileInfo{ .module_path    = "modules/globals",
			                .package_prefix = "graph_consistency_globals" },
			PrecompileInfo{ .module_path    = "modules/globals_initialization",
			                .package_prefix = "graph_consistency_globals_init" },
			PrecompileInfo{ .module_path    = "modules/import_simple",
			                .package_prefix = "graph_consistency_import_simple" }
		};

		std::size_t precompile_suffix = 0;
		for (const auto& info: PRECOMPILE_MODULES) {
			std::string package_id = info.package_prefix;
			package_id += "_";
			package_id += std::to_string(precompile_suffix++);

			global_state::PackageInfo package_info{
				.root_module
				= frontend::createModuleTree(fs::File(path(info.module_path)), package_id),
			};

			driver::compileEntirePackage(
				package_info,
				driver::BackendType::LLVM,
				{ .external_static_libraries = {}, .link_c_standard_library = true }
			);
		}

		// ================================================================================
		// Test setup
		// ================================================================================

		// Get the graph before optimization
		auto graph_before_opt = query::internal::ContextAccess::getState()->getGraphMutable();

		// Collect stable nodes BEFORE optimization (note then Input nodes are included)
		std::vector<query::internal::NodeID> stable_nodes_before;
		for (const auto& node: graph_before_opt->getAllNodes())
			if (node.q_id.getData().usesStableHashing()) stable_nodes_before.push_back(node);

		// Collect stable dependencies for each stable node BEFORE optimization
		// This also include input dependencies
		base::HashMap<query::internal::NodeID, std::vector<query::internal::NodeID>>
			stable_deps_before;
		for (const auto& stable_node: stable_nodes_before) {
			stable_deps_before.put(
				stable_node, collect_stable_dependencies(*graph_before_opt, stable_node)
			);
		}

		// Call serializeQueryGraph and then deserialize to get opt_graph
		auto opt_graph = query::internal::QueryGraph::deserialize(query::external::serializeQueryGraph());

		// Build parent map for the optimized graph
		auto parents = build_parent_map(opt_graph);

		// Get all nodes in the graph
		auto all_nodes = opt_graph.getAllNodes();

		// ================================================================================
		// CHECK 1: - Graph consistency - all children referenced in the graph must exist as keys in
		// the graph
		// ================================================================================
		for (const auto& node: all_nodes) {
			const auto& deps = opt_graph.getDirectDependencies(node);
			for (const auto& child: deps) {
				assertTrue(
					opt_graph.nodeExists(child),
					base::strConcat(
						"Graph inconsistency: child node referenced but does not exist as key in "
						"graph"
					)
				);
			}
		}

		// ================================================================================
		// CHECK 2: Only stable nodes can have no parents (be roots)
		// All non-stable nodes must have at least two parents
		// This is because unstable Node with one (or zero) parents can be removed
		// ================================================================================
		for (const auto& node: all_nodes) {
			CORE_ASSERT(parents.contains(node), "Node not found in parents map");
			const auto& parent_opt = parents.at(node);

			if (!node.q_id.getData().usesStableHashing()) {
				assertTrue(
					parent_opt.size() > 1,
					base::strConcat("Graph not-optimal: non-stable node has less than a two parents"
				                    "Only stable nodes can be roots or have only one parent")
				);
			}
		}

		// ================================================================================
		// CHECK 3: All non-stable nodes must have MORE than one child (cannot have 0 or 1 children)
		// This is because nodes with 0 or 1 children should be trimmed/collapsed during
		// optimization As they can be easy optimised
		// ================================================================================
		for (const auto& node: all_nodes) {
			if (node.q_id.getData().usesStableHashing()) { continue; }  // Skip stable nodes

			const auto& deps = opt_graph.getDirectDependencies(node);
			// Non-stable, non-input nodes must have MORE than one child
			// Nodes with 0 children are leaves and should be trimmed
			// Nodes with 1 child should be collapsed into their parent
			assertTrue(
				deps.size() > 1,
				base::strConcat(
					"Graph not-optimal: non-stable node '",
					node.q_id.getData().name,
					"' has ",
					std::to_string(deps.size()),
					" children. Such nodes must have more than 1 child after optimization."
				)
			);
		}

		// ================================================================================
		// CHECK 4: All stable nodes from before optimization
		// must still exist in the graph after optimization
		// ================================================================================
		for (const auto& stable_node: stable_nodes_before) {
			assertTrue(
				opt_graph.nodeExists(stable_node),
				base::strConcat("Graph inconsistency: stable node was removed during optimization. "
			                    "Stable nodes must be preserved.")
			);
		}

		// ================================================================================
		// CHECK 5: For each stable node, its stable dependencies (this includes inputs)
		// must be preserved after optimization (stable-to-stable edges are never removed)
		// ================================================================================
		for (const auto& stable_node: stable_nodes_before) {
			auto stable_deps_after      = collect_stable_dependencies(opt_graph, stable_node);
			auto stable_deps_before_opt = stable_deps_before.atMaybe(stable_node);

			// Nodes should not be added - if in the fueature we allow that, this test needs to be
			// updated
			ASSERT_TRUE(stable_deps_before_opt.has_value());

			// All stable dependencies from before must still exist after optimization
			// hthissi include input nodes
			assertTrue(
				are_node_vectors_same(*stable_deps_before_opt.value(), stable_deps_after),
				base::strConcat(
					"Graph inconsistency: stable dependencies of stable node '",
					stable_node.q_id.getData().name
				)
			);
		}
	}

	void objFileGenerated() {
		using namespace compiler;

		auto module
			= frontend::createModuleTree(fs::File(path("modules/functions_1")), package_name);

		query::utils::withContextDo([&](query::Context& ctx) {
			// This method can fail on module verification
			auto module_o = ctx.query<driver::CompileModule>({ module, driver::BackendType::LLVM })
			                    .valueOrPanic();

			ASSERT_TRUE(module_o.FILE.exists());

			fs::FileManager::deleteFile(module_o.FILE);
		});
	}

	void assemblyAndLLVMGenerated() {
		using namespace compiler;

		compiler::driver::llvm_dump_ir  = true;
		compiler::driver::llvm_dump_asm = true;
		defer(compiler::driver::llvm_dump_ir = false; compiler::driver::llvm_dump_asm = false;);

		auto module
			= frontend::createModuleTree(fs::File(path("modules/functions_2")), package_name);

		query::utils::withContextDo([&](query::Context& ctx) {
			// This method can fail on module verification
			auto module_o = ctx.query<driver::CompileModule>({ module, driver::BackendType::LLVM });

			auto module_name = base::StrID(
				base::strConcat(
					"module_", frontend::ModuleTree::getPathComponentHash(module).hash.toStringHex()
				)
					.c_str()
			);

			auto asm_file     = module_name.str() + ".s";
			auto llvm_ir_file = module_name.str() + ".ll";

			assertTrue(std::filesystem::exists(asm_file), "Assembly file does not exist");
			assertTrue(std::filesystem::exists(llvm_ir_file), "LLVM IR file does not exist");

			std::filesystem::remove(asm_file);
			std::filesystem::remove(llvm_ir_file);
		});
	}

	void dvmBackendRuns() {
		using namespace compiler;

		auto module
			= frontend::createModuleTree(fs::File(path("modules/functions_3")), package_name);


		query::utils::withContextDo([&](query::Context& ctx) {
			auto run_result = driver::runModuleOnDVM(ctx, module);

			ASSERT_TRUE(run_result.has_value());
			ASSERT_EQUAL_PRINT(0, run_result.value().exit_code);
		});
	}

	void packageCompiles() {
		using namespace compiler;

		// this also checks if llvm IR lib compile and link into the executable:

		global_state::PackageInfo package_info{
			.root_module
			= frontend::createModuleTree(fs::File(path("modules/functions_4")), package_name),
		};

		driver::compileEntirePackage(
			package_info,
			driver::BackendType::LLVM,
			{ .external_static_libraries = {}, .link_c_standard_library = true }
		);

		auto exe_path = artifacts_path / "package_llvm.exe";
		assertTrue(
			std::filesystem::exists(exe_path),
			base::strConcat("Executable file does not exist: ", exe_path.native())
		);

		driver::compileEntirePackage(
			package_info,
			driver::BackendType::DVM,
			{ .external_static_libraries = {}, .link_c_standard_library = true }
		);
	}

	void globalsTest() {
		using namespace compiler;

		auto module = frontend::createModuleTree(fs::File(path("modules/globals")), package_name);

		query::utils::withContextDo([&](query::Context& ctx) {
			// This method can fail on module verification
			auto module_o = ctx.query<driver::CompileModule>({ module, driver::BackendType::LLVM })
			                    .valueOrPanic();

			assertTrue(module_o.FILE.exists(), "Object file does not exist");

			std::filesystem::remove(module_o.FILE.getFilePath().getPath());
		});


		query::utils::withContextDo([&](query::Context& ctx) {
			auto module_dbc = ctx.query<driver::CompileModule>({ module, driver::BackendType::DVM })
			                      .valueOrPanic();
			assertTrue(module_dbc.FILE.exists(), "Object file does not exist");

			std::filesystem::remove(module_dbc.FILE.getFilePath().getPath());

			auto run_result = driver::runModuleOnDVM(ctx, module);
			ASSERT_TRUE(run_result.has_value());
			ASSERT_EQUAL_PRINT(0, run_result.value().exit_code);
		});
	}

	void globalsInitializationTest() {
		using namespace compiler;

		auto module = frontend::createModuleTree(
			fs::File(path("modules/globals_initialization")), package_name
		);

		query::utils::withContextDo([&](query::Context& ctx) {
			auto run_result = driver::runModuleOnDVM(ctx, module);
			ASSERT_TRUE(run_result.has_value());
			ASSERT_EQUAL_PRINT(5, run_result.value().exit_code);
		});
	}

	void saveArtifactsTest() {
		using namespace compiler;

		auto module = frontend::createModuleTree(
			fs::File(path("modules/functions_1")), "artifacts_test_package"
		);

		query::utils::withContextDo([&](query::Context& ctx) {
			(void) ctx.query<driver::CompileModule>({ module, driver::BackendType::LLVM });
		});

		// Serialize current graph
		auto original = query::internal::ContextAccess::getState()->getGraphMutable()->serialize();

		// Call the driver saveArtifacts implementation
		driver::exit();

		// Print path where artifact should have been saved for human inspection
		std::filesystem::path root_path = artifacts_path.getPath();
		auto                  artc_path = root_path / "query" / std::string("query.artc");
		std::cerr << "Query graph artifact path: " << artc_path << '\n';

		// Load an independent ArtifactCollection from disk and read the blob back
		artifacts::ArtifactCollection loaded_root(root_path);
		auto                          query_col = loaded_root.subCollectionAt(base::StrID("query"));
		const auto&                   blob = query_col->blobArtifactAt(base::StrID("query_graph"));
		auto                          view = query_col->getBlobDataView(blob);

		// Deserialize the blob into a QueryGraph and compare with the in-memory graph
		std::span<const byte> span(view.getBegin(), view.size());
		auto                  reloaded = query::internal::QueryGraph::deserialize(span);

		// Get pointer to the in-memory graph we serialized earlier
		auto graph_ptr = query::internal::ContextAccess::getState()->getGraphMutable();

		ASSERT_TRUE(graph_ptr->compare(reloaded));
		ASSERT_TRUE(reloaded.compare(*graph_ptr));
	}

	void sideInputsTest() {
		using namespace compiler;

		global_state::PackageInfo package_info{
			.root_module
			= frontend::createModuleTree(fs::File(path("modules/import_simple")), "import_simple"),
		};

		driver::compileEntirePackage(
			package_info,
			driver::BackendType::LLVM,
			{ .external_static_libraries = {}, .link_c_standard_library = true }
		);

		// Get root module ID
		auto root_id = package_info.root_module;

		// Find submodule ID
		auto root_ref   = frontend::getModuleRef(root_id);
		auto submodules = root_ref->getSubmodules();
		auto get_ref    = [](frontend::AccessLocked<frontend::ModuleID> access) {
            return compiler::frontend::GetModuleID_Functor::
                getModRefUseOnlyWhenYouKnowWhatYouAreDoingThisCanModifyInput(
                    access.illegalAccess().getID()
                );
		};
		auto get_submodule = [&](const std::vector<frontend::ModuleAccessLocked>& subs,
		                         base::StrID name) -> frontend::ModuleAccessLocked {
			for (const auto& sub: subs)
				if (get_ref(sub)->getName() == name) return sub;
			throw std::out_of_range("Submodule not found");
		};
		auto has_submodule
			= [&](const std::vector<frontend::ModuleAccessLocked>& subs, base::StrID name) -> bool {
			for (const auto& sub: subs)
				if (get_ref(sub)->getName() == name) return true;
			return false;
		};
		ASSERT_TRUE(has_submodule(submodules, base::StrID("submodule")));
		auto submodule_id
			= get_submodule(submodules, base::StrID("submodule")).illegalAccess().getID();

		ASSERT_TRUE(has_submodule(submodules, base::StrID("empty_sub_module")));
		auto empty_sub_module_id
			= get_submodule(submodules, base::StrID("empty_sub_module")).illegalAccess().getID();

		// Get Query Graph
		auto graph = query::internal::ContextAccess::getState()->getGraphMutable();

		// Find QueryModuleHOUT node
		auto hout_node_id = query::internal::makeNodeID<helios::QueryModuleHOUT>(root_id);

		// Check dependencies
		auto dependencies = graph->getNodeDeps(hout_node_id);

		// Construct expected SideInput query ID
		auto expected_module_side_input_id
			= query::internal::makeNodeID<frontend::QueryModuleSideInput>(
				frontend::KeyOf_ModuleSideInput{ submodule_id }
			);

		auto submodule_file_id
			= frontend::getModuleRef(submodule_id)->getMainSourceFile().illegalAccess().getID();
		auto expected_file_side_input_id
			= query::internal::makeNodeID<frontend::QueryFileSideInput>(
				frontend::KeyOf_FileSideInput{ submodule_file_id }
			);

		auto unexpected_module_side_input_id
			= query::internal::makeNodeID<frontend::QueryModuleSideInput>(
				frontend::KeyOf_ModuleSideInput{ empty_sub_module_id }
			);

		auto empty_sub_module_file_id = frontend::getModuleRef(empty_sub_module_id)
		                                    ->getMainSourceFile()
		                                    .illegalAccess()
		                                    .getID();
		auto unexpected_file_side_input_id
			= query::internal::makeNodeID<frontend::QueryFileSideInput>(
				frontend::KeyOf_FileSideInput{ empty_sub_module_file_id }
			);

		bool found_module            = false;
		bool found_file              = false;
		bool found_unexpected_module = false;
		bool found_unexpected_file   = false;

		for (auto dep_id: dependencies) {
			if (dep_id == expected_module_side_input_id) found_module = true;
			if (dep_id == expected_file_side_input_id) found_file = true;
			if (dep_id == unexpected_module_side_input_id) found_unexpected_module = true;
			if (dep_id == unexpected_file_side_input_id) found_unexpected_file = true;
		}

		ASSERT_TRUE(found_module);
		ASSERT_TRUE(found_file);
		// found_unexpected_module id found because we are getting the submodules of the root module
		// to find the submodule. In the feature we might want to lookup for submodules with specyfic
		// name without getting all submodules first. It will reduce the number of dependencies.
		ASSERT_TRUE(found_unexpected_module);
		// but we do not depend of the module file, because we are reading only correct
		// "submodule.dmf" file.
		ASSERT_TRUE(!found_unexpected_file);
	}
};


TESTER_COMMON_MAIN("/src/compiler/driver/driver/tests/")
