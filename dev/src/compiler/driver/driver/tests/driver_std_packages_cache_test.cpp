#include <driver/diagnostics/log_helpers.hpp>
#include <driver/exit.hpp>
#include <driver/initialize.hpp>
#include <driver/manifest/manifest.hpp>
#include <driver/operations/generic_operations.hpp>
#include <driver/standard_library/standard_library.hpp>
#include <driver/task/task.hpp>
#include <driver_private/standard_library/standard_library.hpp>
#include <global_state/backend_options.hpp>
#include <global_state/global_logger.hpp>
#include <global_state/packages.hpp>

#include <base/pointers/box.hpp>
#include <base/str/str_utils.hpp>

#include <diagnostic/logger.hpp>
#include <filesystem/file_path.hpp>
#include <tester/tester.hpp>

#include <json/json.hpp>

#include <algorithm>
#include <fstream>
using namespace compiler;

class StdPackagesCacheTest final: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS StdPackagesCacheTest

	fs::FilePath artifacts_path     = fs::FileManager::createRandomTempDirectory().getFilePath();
	fs::FilePath std_artifacts_path = fs::FileManager::createRandomTempDirectory().getFilePath();
	driver::PackageCompilationManifest manifest{};

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(verifyCustomArtifactsCollectionWiring);
		TESTER_ADD_TEST(compileStdPackages);
		TESTER_ADD_TEST(verifyStdLibTasksSkippedOnceArtifactsArePresent);
	}

protected:
	/**
	 * @brief Global setup executed once before all tests.
	 *
	 * - Initializes global logger
	 * - Initializes the compiler in package compilation mode with a custom
	 *   std artifacts cache path
	 */
	void beforeAll() override {
		global_state::setters::setGlobalLogger(makeBox<dia::Logger>());
		const auto     manifest_path = fs::FilePath(path("modules/packages/manifest.json"));
		std::ifstream  in(manifest_path.getPath());
		nlohmann::json manifest_json = nlohmann::json::parse(in, nullptr, true, true);
		auto           manifest_opt  = driver::PackageCompilationManifest::fromJson(
            manifest_json, compiler::driver::diagnostics::makeGlobalLoggerReporter()
        );
		assertTrue(manifest_opt.has_value(), "Failed to parse packages manifest");
		assertTrue(
			manifest_opt->verify(compiler::driver::diagnostics::makeGlobalLoggerReporter()).isOk(),
			"Manifest verification failed"
		);
		auto manifest = std::move(*manifest_opt);


		for (auto& package_info: manifest.packages)
			package_info.package_path = fs::FilePath(path(package_info.package_path.string()));

		auto init_result = compiler::driver::initializeTheCompiler(
			compiler::driver::CompilerModeOfOperationAndOptions::PackageCompilationMode{
				.packages_info = manifest.packages,
				.compilation_artifacts = {
					.artifacts_path = artifacts_path,
				},
				.backend_options = {
					.llvm_backend = global_state::BackendOptions::LLVMBackend{},
				},
				.debug_options         = {},
				.incremental           = {},
				.execution_options     = { .worker_count = 1 },
				.stdlib_options
				= { .std_lib_type = driver::options_types::StdLibOptions::DefaultStd{},
				    .std_artifacts_path = std_artifacts_path },
			}
		);
		assertTrue(init_result.status().isOk(), "Compiler initialization failed");
	}

	void afterAll() override {
		compiler::driver::exit();
		std::filesystem::remove_all(artifacts_path.getPath());
		std::filesystem::remove_all(std_artifacts_path.getPath());
	}

private:
	/**
	 * @brief Verifies that a custom std artifacts path (`stdlib_options.std_artifacts_path`)
	 * both sets the global custom artifacts collection during initialization, and gets
	 * propagated as the artifact collection root of the scheduled std lib compilation tasks.
	 */
	void verifyCustomArtifactsCollectionWiring() {
		auto std_art_collection_opt = global_state::getStdArtifactsCollection();
		ASSERT_HAS_VALUE(std_art_collection_opt);

		auto tasks = driver::getRequiredStdLibCompilationTasks();
		assertTrue(!tasks.empty(), "Expected std lib compilation tasks to be scheduled");

		for (const auto& task: tasks) {
			base::Optional<Ref<artifacts::ArtifactCollection>> custom_art_collection;
			if (auto* dvm_target = std::get_if<driver::BuildTargetDVMLibrary>(&task.build_target))
				custom_art_collection = dvm_target->custom_art_collection;
			else if (auto* lib_target
			         = std::get_if<driver::BuildTargetLLVMStaticLibrary>(&task.build_target))
				custom_art_collection = lib_target->custom_art_collection;
			else
				assertTrue(false, "Unexpected build target type for a std lib compilation task");

			ASSERT_HAS_VALUE(custom_art_collection);
			assertTrue(
				custom_art_collection->get() == std_art_collection_opt->get(),
				"Task's artifact collection root does not match the custom std artifacts "
				"collection"
			);
		}
	}

	/**
	 * @brief Compiles all standard library packages and verifies output artifacts.
	 */
	void compileStdPackages() {
		auto tasks = driver::getRequiredStdLibCompilationTasks();

		// Run standard library compilation
		ASSERT_TRUE(driver::compilePackages(tasks).isOk());

		// Verify expected artifacts
		for (const auto& art: driver::getStdLibNativeArtifacts()) {
			assertTrue(
				std::filesystem::exists(art.file.getFilePath().getPath()),
				base::strConcat(
					"Missing standard library static library artifact: ",
					art.file.getFilePath().getPath().string()
				)
			);
		}
	}

	/**
	 * @brief Once all std lib artifacts have been compiled into the custom artifacts
	 * directory, requesting the tasks again should find them already present and
	 * skip recompilation entirely (empty task list).
	 */
	void verifyStdLibTasksSkippedOnceArtifactsArePresent() {
		auto tasks = driver::getRequiredStdLibCompilationTasks();
		assertTrue(
			tasks.empty(),
			"Expected no std lib compilation tasks once artifacts are already present in the "
			"custom directory"
		);
	}
};

TESTER_COMMON_MAIN("/src/compiler/driver/driver/tests/")
