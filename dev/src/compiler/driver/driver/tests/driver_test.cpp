#include <driver/exit.hpp>
#include <driver/initialize.hpp>
#include <driver/module_flags/module_flags.hpp>
#include <driver/operations/generic_operations.hpp>
#include <frontend/module_tree/functors.hpp>
#include <frontend/module_tree/module_tree.hpp>
#include <frontend/module_tree/queries.hpp>
#include <global_state/backend_options.hpp>
#include <global_state/packages.hpp>
#include <helios/queries.hpp>

#include <artifacts/artifacts.hpp>
#include <filesystem/file_path.hpp>
#include <query_framework/internal/context_access.hpp>
#include <query_framework/internal/query_graph/query_graph.hpp>
#include <query_framework/query_entry_point.hpp>
#include <query_framework/utils/with_context_do.hpp>
#include <string_id/string_id.hpp>
#include <tester/tester.hpp>

#include <filesystem>

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
				.backend_options = {
					.llvm_backend = global_state::BackendOptions::LLVMBackend{},
				},
				.debug_options         = {},
				.incremental           = {}
			}
		);
	}

private:
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
