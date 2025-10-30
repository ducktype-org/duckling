#include <driver/exit.hpp>
#include <driver/initialize.hpp>
#include <driver/mark_nodes.hpp>
#include <driver/operations/generic_operations.hpp>
#include <frontend/module_tree/module_tree.hpp>
#include <global_state/options.hpp>
#include <global_state/packages.hpp>

#include <base/str/string_id.hpp>

#include <artifacts/artifacts.hpp>
#include <filesystem/file_path.hpp>
#include <query_framework/internal/context_access.hpp>
#include <query_framework/internal/query_graph/query_graph.hpp>
#include <query_framework/utils/with_context_do.hpp>
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
		TESTER_ADD_TEST(collectPstHashesTest);
		TESTER_ADD_TEST(loadPreviousGraphTest);

		compiler::driver::initializeTheCompiler(
			compiler::driver::CompilerModeOfOperationAndOptions::PackageCompilationMode{
				.main_package_info = {
					.package_name = package_name,
					.package_path = fs::FilePath(path("modules/functions_1")),
				},
				.compilation_artifacts = {
					.artifacts_path = artifacts_path,
				},
				.debug_options         = {}
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
			auto module_o = ctx.query<driver::CompileModule>({ module, driver::BackendType::LLVM });

			ASSERT_TRUE(module_o.FILE.exists());

			fs::FileManager::deleteFile(module_o.FILE);
		});
	}

	void assemblyAndLLVMGenerated() {
		using namespace compiler;

		global_state::getDynamicDebugOptions()->llvm_dump_ir  = true;
		global_state::getDynamicDebugOptions()->llvm_dump_asm = true;
		defer(global_state::getDynamicDebugOptions()->llvm_dump_ir  = false;
		      global_state::getDynamicDebugOptions()->llvm_dump_asm = false;);

		auto module
			= frontend::createModuleTree(fs::File(path("modules/functions_2")), package_name);

		query::utils::withContextDo([&](query::Context& ctx) {
			// This method can fail on module verification
			auto module_o = ctx.query<driver::CompileModule>({ module, driver::BackendType::LLVM });

			auto module_name
				= base::StrID(base::strConcat(
								  "module_",
								  frontend::ModuleTree::getComponentHash(module).hash.toStringHex()
				)
			                      .c_str());

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
			.package_name = base::StrID(package_name.c_str()),
			.package_path = fs::FilePath(path("modules/functions_4")),
		};

		driver::compilerEntirePackage(
			package_info,
			driver::BackendType::LLVM,
			{ .external_static_libraries = {}, .link_c_standard_library = true }
		);

		auto exe_path = artifacts_path / "package_llvm.exe";
		assertTrue(
			std::filesystem::exists(exe_path),
			base::strConcat("Executable file does not exist: ", exe_path.native())
		);

		package_info.package_name = base::StrID((package_name + "_dvm").c_str());
		driver::compilerEntirePackage(
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
			auto module_o = ctx.query<driver::CompileModule>({ module, driver::BackendType::LLVM });

			assertTrue(module_o.FILE.exists(), "Object file does not exist");

			std::filesystem::remove(module_o.FILE.getFilePath().getPath());
		});


		query::utils::withContextDo([&](query::Context& ctx) {
			auto module_dbc
				= ctx.query<driver::CompileModule>({ module, driver::BackendType::DVM });
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
		driver::saveArtifacts();

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

	void collectPstHashesTest() {
		using namespace compiler;
		auto hashes = driver::collectAllPstElementHashesFromGlobalPackages();
		std::cerr << "collectAllPstElementHashesFromGlobalPackages returned " << hashes.size()
				  << " hashes\n";
	}

	void loadPreviousGraphTest() {
		using namespace compiler;

		auto module = frontend::createModuleTree(
			fs::File(path("modules/functions_1")), "prev_graph_test_package"
		);

		query::utils::withContextDo([&](query::Context& ctx) {
			(void) ctx.query<driver::CompileModule>({ module, driver::BackendType::LLVM });
		});

		// this is because you can't 'copy' the graph
		auto original_bytes
			= query::internal::ContextAccess::getState()->getGraphMutable()->serialize();
		auto original_graph = query::internal::QueryGraph::deserialize(
			std::span<const byte>(original_bytes.data(), original_bytes.size())
		);

		// Call the driver saveArtifacts implementation to write the graph to artifacts
		driver::saveArtifacts();

		driver::resetInitializationForTests();

		compiler::driver::initializeTheCompiler(
			compiler::driver::CompilerModeOfOperationAndOptions::PackageCompilationMode{
				.main_package_info = {
					.package_name = std::string("prev_graph_test_package"),
					.package_path = fs::FilePath(path("modules/functions_1")),
				},
				.compilation_artifacts = {.artifacts_path = artifacts_path},
				.debug_options         = {}
			}
		);

		// Verify previous graph is present and equals the original
		const auto& prev = query::internal::ContextAccess::getState()->getPreviousGraph();
		ASSERT_TRUE(prev.compare(original_graph));
		ASSERT_TRUE(original_graph.compare(prev));
	}
};


TESTER_COMMON_MAIN("/src/compiler/driver/driver/tests/")
