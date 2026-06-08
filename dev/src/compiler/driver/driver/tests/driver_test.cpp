#include <driver/debug_info/debug_info.hpp>
#include <driver/exit.hpp>
#include <driver/initialize.hpp>
#include <driver/module_flags/module_flags.hpp>
#include <driver/operations/generic_operations.hpp>
#include <frontend/module_tree/functors.hpp>
#include <frontend/module_tree/module_tree.hpp>
#include <frontend/module_tree/queries.hpp>
#include <frontend/pst_parser/source_position_locked.hpp>
#include <global_state/backend_options.hpp>
#include <global_state/packages.hpp>
#include <helios/queries/queries.hpp>
#include <linker/link.hpp>

#include <artifacts/artifacts.hpp>
#include <filesystem/file_path.hpp>
#include <query_framework/entry/query_entry_point.hpp>
#include <query_framework/entry/with_context_do.hpp>
#include <query_framework/external/api.hpp>
#include <query_framework/internal/context_access.hpp>
#include <query_framework/internal/query_graph/query_graph.hpp>
#include <string_id/string_id.hpp>
#include <tester/tester.hpp>

#include <algorithm>
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
		TESTER_ADD_TEST(debugInfoGenerated);
		TESTER_ADD_TEST(assemblyAndLLVMGenerated);
		TESTER_ADD_TEST(dvmBackendRuns);
		TESTER_ADD_TEST(packageCompiles);
		TESTER_ADD_TEST(globalsTest);
		TESTER_ADD_TEST(globalsInitializationTest);
		TESTER_ADD_TEST(saveArtifactsTest);
		TESTER_ADD_TEST(sideInputsTest);
		TESTER_ADD_TEST(moduleChildSideInputsTest);
		TESTER_ADD_TEST(sourcePositionInputDependencyForDvmDebugInfoInCompileEntirePackageTest);
	}

protected:
	void beforeAll() override {
		auto init_result = compiler::driver::initializeTheCompiler(
			compiler::driver::CompilerModeOfOperationAndOptions::PackageCompilationMode{
				.packages_info = {
					{
						.package_name = base::StrID(package_name),
						.version      = base::StrID("not_supported"),
						.package_path  = fs::FilePath(path("modules/functions_1")),
						.features     = {},
						.dependencies = {},
					},
				},
				.compilation_artifacts = {
					.artifacts_path = artifacts_path,
				},
				.backend_options = {
					.llvm_backend = global_state::BackendOptions::LLVMBackend{},
				},
				.debug_options         = { },
				.incremental           = { },
				.execution_options     = { .worker_count = 1 },
				.stdlib_options		= { },
			}
		);
		ASSERT_TRUE(init_result.status().isOk());
	}

private:
	/// @brief The main graph consistency test after serialization/optimization
	void graphConsistencyAfterOptimizationTest() {
		using namespace compiler;

		// ================================================================================
		// Helper lambdas for this test
		// ================================================================================

		auto is_preserved = [](const query::internal::NodeID& node) -> bool {
			return node.q_id.getData().tags.preserve_in_graph;
		};

		// Collects (all) preserved dependencies (transitively) of NodeID
		auto collect_preserved_dependencies
			= [&is_preserved](
				  const query::internal::QueryGraph& graph, const query::internal::NodeID& node
			  ) -> std::vector<query::internal::NodeID> {
			std::vector<query::internal::NodeID> result;

			const auto& deps = graph.getNodeDeps(node);
			for (const auto& dep: deps)
				if (is_preserved(dep)) result.push_back(dep);
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
		auto are_node_vectors_same = [](std::vector<query::internal::NodeID>& first,
		                                std::vector<query::internal::NodeID>& second) -> bool {
			std::ranges::sort(first, [](auto& l, auto& r) { return l < r; });
			std::ranges::sort(second, [](auto& l, auto& r) { return l < r; });
			return first == second;
		};

		auto count_edges = [](const query::internal::QueryGraph& graph) -> std::size_t {
			std::size_t total = 0;
			for (const auto& node: graph.getAllNodes())
				total += graph.getDirectDependencies(node).size();
			return total;
		};

		// ================================================================================
		// Sanity check: this test must run before any other tests
		// ================================================================================
		auto  ctx_state          = query::internal::ContextAccess::getState();
		auto& initial_graph_view = ctx_state->getGraph();
		assertTrue(
			initial_graph_view.getAllNodes().empty(),
			"graphConsistencyAfterOptimizationTest must run before other driver tests to keep "
			"the"
			" compilation graph clean"
		);

		// run the serialization before any compilation to see if it works on empty graph
		auto serialized_empty_graph = query::external::optAndSerializeQueryGraph();
		auto deserialized_empty_graph
			= query::internal::QueryGraph::deserialize(serialized_empty_graph);
		assertTrue(
			deserialized_empty_graph.getAllNodes().empty(), "Deserialized empty graph must be empty"
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
			PrecompileInfo{
				.module_path    = "modules/functions_1",
				.package_prefix = "graph_consistency_functions_1",
			},
			PrecompileInfo{
				.module_path    = "modules/functions_2",
				.package_prefix = "graph_consistency_functions_2",
			},
			PrecompileInfo{
				.module_path    = "modules/functions_3",
				.package_prefix = "graph_consistency_functions_3",
			},
			PrecompileInfo{
				.module_path    = "modules/functions_4",
				.package_prefix = "graph_consistency_functions_4",
			},
			PrecompileInfo{
				.module_path    = "modules/globals",
				.package_prefix = "graph_consistency_globals",
			},
			PrecompileInfo{
				.module_path    = "modules/globals_initialization",
				.package_prefix = "graph_consistency_globals_init",
			},
			PrecompileInfo{
				.module_path    = "modules/import_simple",
				.package_prefix = "graph_consistency_import_simple",
			},
		};

		std::size_t precompile_suffix = 0;
		for (const auto& info: PRECOMPILE_MODULES) {
			std::string package_id = info.package_prefix;
			package_id += "_";
			package_id += std::to_string(precompile_suffix++);

			compiler::frontend::packages::PackageInfo package_info(
				frontend::createModuleTree(
					fs::File(path(info.module_path)), base::StrID(package_id.c_str())
				),
				base::StrID("not_supported"),
				{},
				{}
			);

			driver::compileEntirePackage(
				package_info,
				driver::BuildTargetLLVMExecutable{
					.output_file_stem = base::StrID("package_llvm"),
					.linking_options  = linker::LinkingOptions{
						.linker_path = {},
						.additional_link_options = {},
						.link_c_standard_library = true,
						.stdlib_link_options = {},
					},
				}
			);
		}

		// ================================================================================
		// Test setup
		// ================================================================================

		// Get the graph before optimization
		auto& graph_before_opt = query::internal::ContextAccess::getState()->getGraph();

		// Collect preserved nodes BEFORE optimization (note that Input nodes are included)
		std::vector<query::internal::NodeID> preserved_nodes_before;
		for (const auto& node: graph_before_opt.getAllNodes())
			if (is_preserved(node)) preserved_nodes_before.push_back(node);

		// Collect preserved dependencies (transitively) for each preserved node BEFORE
		// optimization This also includes input dependencies
		base::HashMap<query::internal::NodeID, std::vector<query::internal::NodeID>>
			preserved_deps_before;
		for (const auto& preserved_node: preserved_nodes_before) {
			preserved_deps_before.put(
				preserved_node, collect_preserved_dependencies(graph_before_opt, preserved_node)
			);
		}

		const auto nodes_before_opt = graph_before_opt.getAllNodes().size();
		const auto edges_before_opt = count_edges(graph_before_opt);
		std::cout << "[DriverTest] Graph before optimization: nodes=" << nodes_before_opt
				  << " edges=" << edges_before_opt << '\n';

		// Call optAndSerializeQueryGraph() and then deserialize to get opt_graph
		auto opt_graph
			= query::internal::QueryGraph::deserialize(query::external::optAndSerializeQueryGraph());
		const auto nodes_after_opt = opt_graph.getAllNodes().size();
		const auto edges_after_opt = count_edges(opt_graph);
		std::cout << "[DriverTest] Graph after optimization: nodes=" << nodes_after_opt
				  << " edges=" << edges_after_opt << '\n';

		// Build parent map for the optimized graph
		auto parents = build_parent_map(opt_graph);

		// Get all nodes in the graph
		auto all_nodes = opt_graph.getAllNodes();

		// ================================================================================
		// CHECK 1: Graph consistency - all children referenced in the graph must exist as keys
		// in the graph
		// ================================================================================
		for (const auto& node: all_nodes) {
			auto& deps = opt_graph.getDirectDependencies(node);
			for (const auto& child: deps) {
				assertTrue(
					opt_graph.nodeExists(child),
					base::strConcat("Graph inconsistency: child node referenced but does not "
				                    "exist as key in "
				                    "graph")
				);
			}
			// We also check that deps do not contain duplicates as they can be accidentally
			// added during optimization
			auto deps_sorted = deps;
			std::ranges::sort(deps_sorted, [](auto& l, auto& r) { return l < r; });
			auto dup_it = std::ranges::adjacent_find(deps_sorted);
			assertTrue(
				dup_it == deps_sorted.end(),
				base::strConcat("Graph inconsistency: node has duplicate dependencies listed")
			);
		}

		// ================================================================================
		// CHECK 2: Only preserved nodes can have no parents (be roots)
		// All non-preserved nodes must have at least two parents
		// This is because non-preserved nodes with one (or zero) parents can be removed
		// ================================================================================
		for (const auto& node: all_nodes) {
			CORE_ASSERT(parents.contains(node), "Node not found in parents map");
			const auto& parent_opt = parents.at(node);

			if (!is_preserved(node)) {
				assertTrue(
					parent_opt.size() > 1,
					base::strConcat(
						"Graph not-optimal: non-preserved node has fewer than two parents. "
						"Only preserved nodes can be roots or have only one parent.",
						" Node: ",
						node.q_id.getData().name,
						" Parents count: ",
						std::to_string(parent_opt.size()),
						" Children count: ",
						std::to_string(opt_graph.getDirectDependencies(node).size()),
						" Is preserved: ",
						is_preserved(node) ? "true" : "false"
					)
				);
			}
		}

		// ================================================================================
		// CHECK 3: All non-preserved nodes must have MORE than one child (cannot have 0 or 1
		// children) This is because nodes with 0 or 1 children should be trimmed/collapsed
		// during optimization, as they can be easily optimized
		// ================================================================================
		for (const auto& node: all_nodes) {
			if (is_preserved(node)) { continue; }  // Skip preserved nodes

			const auto& deps = opt_graph.getDirectDependencies(node);
			// Non-preserved nodes must have MORE than one child
			// Nodes with 0 children are leaves and should be trimmed
			// Nodes with 1 child should be collapsed into their parent
			assertTrue(
				deps.size() > 1,
				base::strConcat(
					"Graph not-optimal: non-preserved node '",
					node.q_id.getData().name,
					"' has ",
					std::to_string(deps.size()),
					" children. Such nodes must have more than 1 child after optimization."
				)
			);
		}

		// ================================================================================
		// CHECK 4: All preserved nodes from before optimization
		// must still exist in the graph after optimization
		// ================================================================================
		for (const auto& preserved_node: preserved_nodes_before) {
			assertTrue(
				opt_graph.nodeExists(preserved_node),
				base::strConcat(
					"Graph inconsistency: preserved node was removed during optimization. "
					"Preserved nodes must remain in the graph."
				)
			);
		}

		// ================================================================================
		// CHECK 5: For each preserved node, its preserved dependencies (this includes inputs)
		// must be preserved after optimization (preserved-to-preserved paths are never removed)
		// ================================================================================
		for (const auto& preserved_node: preserved_nodes_before) {
			auto preserved_deps_after = collect_preserved_dependencies(opt_graph, preserved_node);
			auto preserved_deps_before_opt = preserved_deps_before.atMaybe(preserved_node);

			// Nodes should not be added - if in the future we allow that, this test needs to be
			// updated
			ASSERT_TRUE(preserved_deps_before_opt.has_value());

			// All preserved dependencies from before must still exist after optimization.
			// This includes input nodes.
			assertTrue(
				are_node_vectors_same(*preserved_deps_before_opt.value(), preserved_deps_after),
				base::strConcat(
					"Graph inconsistency: preserved dependencies of preserved node '",
					preserved_node.q_id.getData().name
				)
			);
		}

		// ================================================================================
		// CHECK 6: Re-running serialization/optimization must be idempotent
		// ================================================================================
		auto second_opt_graph
			= query::internal::QueryGraph::deserialize(query::external::optAndSerializeQueryGraph());

		auto compare_graphs = [&](const query::internal::QueryGraph& lhs_graph,
		                          const query::internal::QueryGraph& rhs_graph,
		                          const char*                        missing_msg_prefix) {
			for (const auto& node: lhs_graph.getAllNodes()) {
				assertTrue(
					rhs_graph.nodeExists(node),
					base::strConcat(missing_msg_prefix, node.q_id.getData().name)
				);

				auto deps_lhs = lhs_graph.getDirectDependencies(node);
				auto deps_rhs = rhs_graph.getDirectDependencies(node);
				assertTrue(
					are_node_vectors_same(deps_lhs, deps_rhs),
					base::strConcat(
						"Graph inconsistency: dependency mismatch for node '",
						node.q_id.getData().name,
						"' between optimization runs"
					)
				);
			}
		};

		compare_graphs(
			opt_graph, second_opt_graph, "Graph inconsistency: node missing in second graph: "
		);
		compare_graphs(
			second_opt_graph, opt_graph, "Graph inconsistency: node missing in first graph: "
		);
	}

	void objFileGenerated() {
		using namespace compiler;

		auto module = frontend::createModuleTree(
			fs::File(path("modules/functions_1")), base::StrID(package_name)
		);

		query::utils::withContextDo([&](query::Context& ctx) {
			// This method can fail on module verification
			auto artifacts
				= ctx.query<driver::CompileModule>({ module, driver::BackendType::LLVM, false })
			          ->valueOrPanic();

			ASSERT_TRUE(artifacts.object_art.file.exists());
			ASSERT_TRUE(artifacts.debug_info.empty());

			fs::FileManager::deleteFile(artifacts.object_art.file);
		});
	}

	void debugInfoGenerated() {
		using namespace compiler;

		auto module = frontend::createModuleTree(
			fs::File(path("modules/functions_2")), base::StrID(package_name)
		);

		query::utils::withContextDo([&](query::Context& ctx) {
			auto artifacts
				= ctx.query<driver::CompileModule>({ module, driver::BackendType::DVM, true })
			          ->valueOrPanic();

			ASSERT_TRUE(artifacts.object_art.file.exists());
			ASSERT_TRUE(artifacts.debug_info.has_value());

			fs::FileManager::deleteFile(artifacts.object_art.file);
			fs::FileManager::deleteFile(
				artifacts.object_art.file.getFilePath().parentPath()
				/ artifacts.object_art.file.stem().append(driver::DEBUG_INFO_STABLE_EXTENSION)
			);
		});
	}

	void assemblyAndLLVMGenerated() {
		using namespace compiler;

		compiler::driver::dump_ir_options.dump_asm  = true;
		compiler::driver::dump_ir_options.dump_llvm = true;
		compiler::driver::dump_ir_options.dump_lir  = true;
		compiler::driver::dump_ir_options.dump_mir  = true;
		compiler::driver::dump_ir_options.dump_hir  = true;
		defer(compiler::driver::dump_ir_options.dump_asm  = false;
		      compiler::driver::dump_ir_options.dump_llvm = false;
		      compiler::driver::dump_ir_options.dump_lir  = false;
		      compiler::driver::dump_ir_options.dump_mir  = false;
		      compiler::driver::dump_ir_options.dump_hir  = false;);

		auto module = frontend::createModuleTree(
			fs::File(path("modules/functions_3")), base::StrID(package_name)
		);

		query::utils::withContextDo([&](query::Context& ctx) {
			// This method can fail on module verification
			ctx.query<driver::CompileModule>({ module, driver::BackendType::LLVM, false });

			auto                  module_name = base::StrID(base::strConcat(
                "module_",
                compiler::frontend::ModuleTree::getPathComponentHash(module).hash.toStringHex()
            ));
			std::filesystem::path base_path   = artifacts_path / "duck_debug_artifacts";


			auto asm_art  = base_path / (module_name.str() + ".s");
			auto llvm_art = base_path / (module_name.str() + ".ll");
			auto lir_art  = base_path / (module_name.str() + ".lir");
			auto mir_art  = base_path / (module_name.str() + ".mir");
			auto hir_art  = base_path / (module_name.str() + ".hir");


			assertTrue(std::filesystem::exists(asm_art), "Assembly file does not exist");
			assertTrue(std::filesystem::exists(llvm_art), "LLVM IR file does not exist");
			assertTrue(std::filesystem::exists(lir_art), "LIR file does not exist");
			assertTrue(std::filesystem::exists(mir_art), "MIR file does not exist");
			assertTrue(std::filesystem::exists(hir_art), "HIR file does not exist");

			std::filesystem::remove(asm_art);
			std::filesystem::remove(llvm_art);
			std::filesystem::remove(lir_art);
			std::filesystem::remove(mir_art);
			std::filesystem::remove(hir_art);
		});
	}

	void dvmBackendRuns() {
		using namespace compiler;

		auto module = frontend::createModuleTree(
			fs::File(path("modules/functions_4")), base::StrID(package_name)
		);


		query::utils::withContextDo([&](query::Context& ctx) {
			auto run_result = driver::runModuleOnDVM(ctx, module);

			ASSERT_TRUE(run_result.has_value());
			ASSERT_EQUAL_PRINT(0, run_result.value().exit_code);
		});
	}

	void packageCompiles() {
		using namespace compiler;

		// this also checks if llvm IR lib compile and link into the executable:

		compiler::frontend::packages::PackageInfo package_info(
			frontend::createModuleTree(
				fs::File(path("modules/functions_5")), base::StrID(package_name)
			),
			base::StrID("not_supported"),
			{},
			{}
		);

		driver::compileEntirePackage(
			package_info,
			driver::BuildTargetLLVMExecutable{
				.output_file_stem = base::StrID("package_llvm"),
				.linking_options  = linker::LinkingOptions{
					.linker_path = {},
					.additional_link_options = {},
					.link_c_standard_library = true,
					.stdlib_link_options = {}
				},
			}
		);

		auto exe_path = artifacts_path / "package_llvm.exe";
		assertTrue(
			std::filesystem::exists(exe_path),
			base::strConcat("Executable file does not exist: ", exe_path.native())
		);

		driver::compileEntirePackage(package_info, driver::BuildTargetDVM{});

		auto dvm_exe_path = artifacts_path / "package_dvm.dbc";
		assertTrue(
			std::filesystem::exists(dvm_exe_path),
			base::strConcat("DVM executable file does not exist: ", dvm_exe_path.native())
		);
	}

	void globalsTest() {
		using namespace compiler;

		auto module = frontend::createModuleTree(
			fs::File(path("modules/globals")), base::StrID(package_name)
		);

		query::utils::withContextDo([&](query::Context& ctx) {
			// This method can fail on module verification
			auto artifacts
				= ctx.query<driver::CompileModule>({ module, driver::BackendType::LLVM, false })
			          ->valueOrPanic();

			assertTrue(artifacts.object_art.file.exists(), "Object file does not exist");

			std::filesystem::remove(artifacts.object_art.file.getFilePath().getPath());
		});


		query::utils::withContextDo([&](query::Context& ctx) {
			auto artifacts
				= ctx.query<driver::CompileModule>({ module, driver::BackendType::DVM, false })
			          ->valueOrPanic();
			assertTrue(artifacts.object_art.file.exists(), "Object file does not exist");

			std::filesystem::remove(artifacts.object_art.file.getFilePath().getPath());

			auto run_result = driver::runModuleOnDVM(ctx, module);
			ASSERT_TRUE(run_result.has_value());
			ASSERT_EQUAL_PRINT(0, run_result.value().exit_code);
		});
	}

	void globalsInitializationTest() {
		using namespace compiler;

		auto module = frontend::createModuleTree(
			fs::File(path("modules/globals_initialization")), base::StrID(package_name)
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
			fs::File(path("modules/functions_1")), base::StrID("artifacts_test_package")
		);

		query::utils::withContextDo([&](query::Context& ctx) {
			(void) ctx.query<driver::CompileModule>({ module, driver::BackendType::LLVM, false });
		});

		// Serialize current graph
		auto original
			= query::internal::QueryGraph::deserialize(query::external::optAndSerializeQueryGraph());

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

		ASSERT_TRUE(original.compare(reloaded));
		ASSERT_TRUE(reloaded.compare(original));
	}

	void sideInputsTest() {
		using namespace compiler;

		compiler::frontend::packages::PackageInfo package_info(
			frontend::createModuleTree(
				fs::File(path("modules/import_simple")), base::StrID("import_simple")
			),
			base::StrID("not_supported"),
			{},
			{}
		);

		driver::compileEntirePackage(
			package_info,
			driver::BuildTargetLLVMExecutable{
				.output_file_stem = base::StrID("package_llvm"),
				.linking_options  = linker::LinkingOptions{
					.linker_path = {},
					.additional_link_options = {},
					.link_c_standard_library = true,
					.stdlib_link_options = {}
				},
			}
		);

		// Get root module ID
		auto root_id = package_info.getRootModule().illegalAccess().getID();

		// Find submodule ID
		auto root_ref   = frontend::getModuleRef(root_id);
		auto submodules = root_ref->getSubmodules().illegalAccess();
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
		auto& graph = query::internal::ContextAccess::getState()->getGraph();

		// Find QueryModuleHOUT node
		auto hout_node_id = query::internal::makeNodeID<helios::QueryModuleHOUT>(root_id);

		// Check dependencies
		auto dependencies = graph.getNodeDeps(hout_node_id);

		// Construct expected SideInput query ID
		auto expected_module_side_input_id
			= query::internal::makeNodeID<frontend::QueryModuleSideInput>(
				frontend::KeyOf_ModuleSideInput{ frontend::ModuleTree::getModuleHash(submodule_id) }
			);

		auto submodule_file_id
			= frontend::getModuleRef(submodule_id)->getMainSourceFile().illegalAccess().getID();
		auto expected_file_side_input_id
			= query::internal::makeNodeID<frontend::QueryFileSideInput>(
				frontend::KeyOf_FileSideInput{
					frontend::getFileRef(submodule_file_id)->getComponentHash().hash }
			);

		auto unexpected_module_side_input_id
			= query::internal::makeNodeID<frontend::QueryModuleSideInput>(
				frontend::KeyOf_ModuleSideInput{
					frontend::ModuleTree::getModuleHash(empty_sub_module_id) }
			);

		auto empty_sub_module_file_id = frontend::getModuleRef(empty_sub_module_id)
		                                    ->getMainSourceFile()
		                                    .illegalAccess()
		                                    .getID();
		auto unexpected_file_side_input_id
			= query::internal::makeNodeID<frontend::QueryFileSideInput>(
				frontend::KeyOf_FileSideInput{
					frontend::getFileRef(empty_sub_module_file_id)->getComponentHash().hash }
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
		// We should not depend on unrelated submodules when resolving imports by name.
		ASSERT_TRUE(!found_unexpected_module);
		// but we do not depend of the module file, because we are reading only correct
		// "submodule.dmf" file.
		ASSERT_TRUE(!found_unexpected_file);
	}

	/**
	 * @brief Tests the correctness of ModuleChildSideInput,
	 *        and SubmoduleCountSideInput dependencies in the query graph.
	 *
	 * This test compiles the imports_complicated module structure and verifies that:
	 * 1. Each module depends on the correct ModuleChildSideInputs based on its imports
	 * 2. No module depends on SubmoduleCountSideInput (currently not used)
	 */
	void moduleChildSideInputsTest() {
		using namespace compiler;

		compiler::frontend::packages::PackageInfo package_info(
			frontend::createModuleTree(
				fs::File(path("modules/imports_complicated")),
				base::StrID("imports_complicated_test")
			),
			base::StrID("not_supported"),
			{},
			{}
		);

		driver::compileEntirePackage(
			package_info,
			driver::BuildTargetLLVMExecutable{
				.output_file_stem = base::StrID("package_llvm"),
				.linking_options  = linker::LinkingOptions{
					.linker_path = {},
					.additional_link_options = {},
					.link_c_standard_library = true,
					.stdlib_link_options = {}
				},
			}
		);

		// ================================================================================
		// Helper lambdas
		// ================================================================================

		auto root_id  = package_info.getRootModule().illegalAccess().getID();
		auto root_ref = frontend::getModuleRef(root_id);

		// Helper to get mutable module reference
		auto get_ref = [](frontend::AccessLocked<frontend::ModuleID> access) {
			return compiler::frontend::GetModuleID_Functor::
				getModRefUseOnlyWhenYouKnowWhatYouAreDoingThisCanModifyInput(
					access.illegalAccess().getID()
				);
		};

		// Helper to find a submodule by name
		auto get_submodule = [&](const std::vector<frontend::ModuleAccessLocked>& subs,
		                         base::StrID name) -> frontend::ModuleAccessLocked {
			for (const auto& sub: subs)
				if (get_ref(sub)->getName() == name) return sub;
			throw std::out_of_range("Submodule not found: " + std::string(name.strView()));
		};

		// Helper to get all transitive dependencies of a module's HOUT node
		auto get_module_deps = [&](frontend::ModuleID module_id) {
			const auto& graph   = query::internal::ContextAccess::getState()->getGraph();
			auto        hout_id = query::internal::makeNodeID<helios::QueryModuleHOUT>(module_id);
			return graph.getNodeDeps(hout_id);
		};

		// Helper to check if module depends on a specific ModuleChildSideInput
		auto has_child_side_input_dep = [&](const std::vector<query::internal::NodeID>& deps,
		                                    frontend::ModuleID parent_module,
		                                    base::StrID        child_name,
		                                    bool               found) -> bool {
			auto expected_id = query::internal::makeNodeID<frontend::QueryModuleChildSideInput>(
				frontend::KeyOf_ModuleChildSideInput{
					.parent_hash = frontend::ModuleTree::getModuleHash(parent_module),
					.child_name  = child_name,
					.found       = found }
			);
			return std::ranges::find(deps, expected_id) != deps.end();
		};

		// Helper to count how many ModuleChildSideInput dependencies a module has
		auto count_child_side_inputs
			= [](const std::vector<query::internal::NodeID>& deps) -> usize {
			usize count = 0;
			for (const auto& dep: deps)
				if (dep.q_id.asInt() == frontend::QueryModuleChildSideInput::getID().asInt())
					count++;
			return count;
		};


		// Helper to count how many SubmoduleCountSideInput dependencies exist
		auto count_submodule_count_inputs
			= [](const std::vector<query::internal::NodeID>& deps) -> usize {
			usize count = 0;
			for (const auto& dep: deps)
				if (dep.q_id.asInt() == frontend::QuerySubmoduleCountSideInput::getID().asInt())
					count++;
			return count;
		};

		// ================================================================================
		// Build module ID map
		// ================================================================================

		// Root: imports_complicated
		auto imports_complicated_id = root_id;

		// Level 1: foo, bar
		auto foo_subs = root_ref->getSubmodules().illegalAccess();
		auto foo_id   = get_submodule(foo_subs, base::StrID("foo")).illegalAccess().getID();
		auto bar_id   = get_submodule(foo_subs, base::StrID("bar")).illegalAccess().getID();

		// Level 2 under foo: A, B
		auto foo_ref      = frontend::getModuleRef(foo_id);
		auto foo_children = foo_ref->getSubmodules().illegalAccess();
		auto a_id         = get_submodule(foo_children, base::StrID("A")).illegalAccess().getID();
		auto b_id         = get_submodule(foo_children, base::StrID("B")).illegalAccess().getID();

		// Level 2 under bar: C, D
		auto bar_ref      = frontend::getModuleRef(bar_id);
		auto bar_children = bar_ref->getSubmodules().illegalAccess();
		auto c_id         = get_submodule(bar_children, base::StrID("C")).illegalAccess().getID();
		auto d_id         = get_submodule(bar_children, base::StrID("D")).illegalAccess().getID();

		// ================================================================================
		// Get dependencies for each module
		// ================================================================================

		auto b_deps                   = get_module_deps(b_id);
		auto foo_deps                 = get_module_deps(foo_id);
		auto a_deps                   = get_module_deps(a_id);
		auto imports_complicated_deps = get_module_deps(imports_complicated_id);
		auto bar_deps                 = get_module_deps(bar_id);
		auto c_deps                   = get_module_deps(c_id);
		auto d_deps                   = get_module_deps(d_id);

		// ================================================================================
		// Test Module B's ChildSideInput dependencies
		// B.dmf imports: imports_complicated.bar.D
		// Expected: B -> imports_complicated (false), imports_complicated -> bar (true), bar ->
		// D (true)
		// ================================================================================

		ASSERT_TRUE(has_child_side_input_dep(b_deps, b_id, base::StrID("imports_complicated"), false)
		);
		ASSERT_TRUE(
			has_child_side_input_dep(b_deps, imports_complicated_id, base::StrID("bar"), true)
		);
		ASSERT_TRUE(has_child_side_input_dep(b_deps, bar_id, base::StrID("D"), true));
		ASSERT_EQUAL_PRINT(3, count_child_side_inputs(b_deps));

		// ================================================================================
		// Test Module foo's ChildSideInput dependencies
		// foo.dmf imports: imports_complicated.bar, A, B
		// Expected: foo -> A (true), foo -> B (true), foo -> imports_complicated (false),
		//           imports_complicated -> bar (true), B -> imports_complicated (false),
		//           bar -> D (true), bar -> C (true)
		// ================================================================================

		ASSERT_TRUE(has_child_side_input_dep(foo_deps, foo_id, base::StrID("A"), true));
		ASSERT_TRUE(has_child_side_input_dep(foo_deps, foo_id, base::StrID("B"), true));
		ASSERT_TRUE(
			has_child_side_input_dep(foo_deps, foo_id, base::StrID("imports_complicated"), false)
		);
		ASSERT_TRUE(
			has_child_side_input_dep(foo_deps, imports_complicated_id, base::StrID("bar"), true)
		);
		// From B's transitive imports
		ASSERT_TRUE(
			has_child_side_input_dep(foo_deps, b_id, base::StrID("imports_complicated"), false)
		);
		ASSERT_TRUE(has_child_side_input_dep(foo_deps, bar_id, base::StrID("D"), true));
		ASSERT_TRUE(has_child_side_input_dep(foo_deps, bar_id, base::StrID("C"), true));
		ASSERT_EQUAL_PRINT(7, count_child_side_inputs(foo_deps));

		// ================================================================================
		// Test Module A's ChildSideInput dependencies
		// A.dmf imports: foo
		// Expected: A -> foo (false) and nothing else
		// ================================================================================

		ASSERT_TRUE(has_child_side_input_dep(a_deps, a_id, base::StrID("foo"), false));
		ASSERT_EQUAL_PRINT(1, count_child_side_inputs(a_deps));

		// ================================================================================
		// Test Module imports_complicated's ChildSideInput dependencies
		// imports_complicated.dmf imports: bar
		// Expected: imports_complicated -> bar (true), bar -> C (true)
		// ================================================================================

		ASSERT_TRUE(has_child_side_input_dep(
			imports_complicated_deps, imports_complicated_id, base::StrID("bar"), true
		));
		ASSERT_TRUE(
			has_child_side_input_dep(imports_complicated_deps, bar_id, base::StrID("C"), true)
		);
		ASSERT_EQUAL_PRINT(2, count_child_side_inputs(imports_complicated_deps));

		// ================================================================================
		// Test Module bar's ChildSideInput dependencies
		// bar.dmf imports: C
		// Expected: bar -> C (true) only
		// ================================================================================

		ASSERT_TRUE(has_child_side_input_dep(bar_deps, bar_id, base::StrID("C"), true));
		ASSERT_EQUAL_PRINT(1, count_child_side_inputs(bar_deps));
		// ================================================================================
		// Test Module C's ChildSideInput dependencies
		// C.dmf has no imports
		// Expected: no ChildSideInput dependencies
		// ================================================================================

		ASSERT_EQUAL_PRINT(0, count_child_side_inputs(c_deps));

		// ================================================================================
		// Test Module D's ChildSideInput dependencies
		// D.dmf has no imports
		// Expected: no ChildSideInput dependencies
		// ================================================================================

		ASSERT_EQUAL_PRINT(0, count_child_side_inputs(d_deps));


		// ================================================================================
		// Test SubmoduleCountSideInput dependencies
		// Currently, no module should depend on SubmoduleCountSideInput.
		// If this changes in the future (e.g., when iterating over all submodules),
		// please update this test accordingly.
		// ================================================================================

		ASSERT_EQUAL_PRINT(0, count_submodule_count_inputs(b_deps));
		ASSERT_EQUAL_PRINT(0, count_submodule_count_inputs(foo_deps));
		ASSERT_EQUAL_PRINT(0, count_submodule_count_inputs(a_deps));
		ASSERT_EQUAL_PRINT(0, count_submodule_count_inputs(imports_complicated_deps));
		ASSERT_EQUAL_PRINT(0, count_submodule_count_inputs(bar_deps));
		ASSERT_EQUAL_PRINT(0, count_submodule_count_inputs(c_deps));
		ASSERT_EQUAL_PRINT(0, count_submodule_count_inputs(d_deps));
	}

	void sourcePositionInputDependencyForDvmDebugInfoInCompileEntirePackageTest() {
		using namespace compiler;

		auto source_position_input = pst::SourcePositionLocked::getQueryInputNode();
		auto source_position_node  = query::internal::NodeID(
            source_position_input.q_id, query::internal::KeyHash{ source_position_input.hash }
        );

		auto has_source_position_dep = [&](const query::internal::NodeID& node_id) {
			auto& graph = query::internal::ContextAccess::getState()->getGraph();
			if (!graph.nodeExists(node_id)) return false;
			auto deps = graph.getNodeDeps(node_id);
			return std::ranges::find(deps, source_position_node) != deps.end();
		};

		compiler::frontend::packages::PackageInfo dvm_package_info(
			frontend::createModuleTree(
				fs::File(path("modules/functions_2")), base::StrID("src_pos_dvm")
			),
			base::StrID("not_supported"),
			{},
			{}
		);

		driver::compileEntirePackage(dvm_package_info, driver::BuildTargetDVM{});
		auto dvm_compile_node
			= query::internal::makeNodeID<driver::CompileModule>(driver::KeyOf_CompileModule{
				.module_id        = dvm_package_info.getRootModule().illegalAccess().getID(),
				.backend_type     = driver::BackendType::DVM,
				.build_debug_info = true,
			});
		auto dvm_debug_node = query::internal::makeNodeID<driver::DebugInfoForModule>(
			driver::KeyOf_DebugInfoForModule{
				.module_id    = dvm_package_info.getRootModule().illegalAccess().getID(),
				.backend_type = driver::BackendType::DVM,
			}
		);
		ASSERT_TRUE(not has_source_position_dep(dvm_compile_node));
		ASSERT_TRUE(has_source_position_dep(dvm_debug_node));
	}
};


TESTER_COMMON_MAIN("/src/compiler/driver/driver/tests/")
