#include <diagnostic_interactive/logger.hpp>
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

#include <filesystem/file_path.hpp>
#include <tester/tester.hpp>

#include <json/json.hpp>

#include <algorithm>
#include <fstream>
#include <string>
using namespace compiler;

class StdPackagesTest final: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS StdPackagesTest

	fs::FilePath artifacts_path = fs::FileManager::createRandomTempDirectory().getFilePath();
	driver::PackageCompilationManifest manifest{};

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(verifyStdPackagesAndDependencies);
		TESTER_ADD_TEST(verifyStdLinkingOptionsInConvertedTasks);
		TESTER_ADD_TEST(compileStdPackages);
	}

protected:
	/**
	 * @brief Global setup executed once before all tests.
	 *
	 * - Initializes global logger
	 * - Initializes the compiler in package compilation mode
	 */
	void beforeAll() override {
		global_state::setters::setGlobalLogger(makeBox<dia_int::Logger>());
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
				.global_linking_options = { .std_lib_type = driver::options_types::GlobalLinkingOptions::DefaultStd{} },
			}
		);
		assertTrue(init_result.status().isOk(), "Compiler initialization failed");
	}

	void afterAll() override {
		compiler::driver::exit();
		std::filesystem::remove_all(artifacts_path.getPath());
	}

private:
	/**
	 * @brief Verifies presence and dependency relationships of standard packages.
	 */
	void verifyStdPackagesAndDependencies() {
		const auto& global_packages = global_state::getPackages();

		// Check that standard library packages exist
		for (const auto& std_pkg_config: driver::STD_PACKAGES_CONFIG) {
			if (std::ranges::none_of(global_packages, [&](const auto& pkg) {
					return pkg.getPackageID() == base::StrID(std_pkg_config.name);
				})) {
				assertTrue(
					false,
					base::strConcat("Standard library package not found: ", std_pkg_config.name)
				);
			}
		}

		// Check that non-std packages have standard library dependencies
		for (const auto& pkg: global_packages) {
			const auto pkg_id = pkg.getPackageID().strView();

			// Skip checking dependencies of the std lib itself
			bool is_std_lib
				= std::ranges::any_of(driver::STD_PACKAGES_CONFIG, [&](const auto& std_pkg_config) {
					  return pkg_id == std_pkg_config.name;
				  });
			if (is_std_lib) continue;

			// Verify it depends on all std packages
			for (const auto& std_pkg_config: driver::STD_PACKAGES_CONFIG) {
				bool depends_on_std = false;
				for (const auto& dep: pkg.getDependencies().illegalAccess()) {
					if (dep.illegalAccess().getPackage().illegalAccess().getID()
					    == base::StrID(std_pkg_config.name)) {
						depends_on_std = true;
						break;
					}
				}
				assertTrue(depends_on_std, "Package is missing dependency on standard library");
			}
		}
	}

	/**
	 * @brief Verifies that standard library linking options are correctly applied to tasks.
	 */
	void verifyStdLinkingOptionsInConvertedTasks() {
		driver::options_types::GlobalLinkingOptions global_opts{
			.std_lib_type = driver::options_types::GlobalLinkingOptions::DefaultStd{}
		};

		for (const auto& raw_task: manifest.tasks) {
			auto task_opt = driver::convertRawTaskToTask(
				raw_task, global_opts, compiler::driver::diagnostics::makeGlobalLoggerReporter()
			);
			ASSERT_TRUE(task_opt.has_value());
			ASSERT_TRUE(task_opt->type == driver::TaskType::PackageCompilation);

			auto task_data = std::get<driver::PackageCompilationTask>(task_opt->task_data);

			if (auto* target_exe
			    = std::get_if<driver::BuildTargetLLVMExecutable>(&task_data.build_target)) {
				// We expect the correct stdlib linking options based on getStdLibLinkingArgs
				auto lib_args = driver::getStdLibLinkingArgs(global_opts);
				ASSERT_TRUE(lib_args.has_value());

				// Ensure the options contain the stdlib args
				assertTrue(
					target_exe->linking_options.stdlib_link_options
						== driver::getStdLibLinkingArgs(global_opts),
					"Converted task linking options do not contain the standard library linking "
					"arguments"
				);
			}
		}
	}

	/**
	 * @brief Compiles all standard library packages and verifies output artifacts.
	 */
	void compileStdPackages() {
		auto tasks = driver::getStandardLibraryCompilationTasks();

		// Run standard library compilation
		ASSERT_TRUE(driver::compilePackages(tasks).isOk());

		// Verify expected artifacts
		for (const auto& config: driver::STD_PACKAGES_CONFIG) {
			auto a_output_path
				= driver::getStdBinariesDirectory() / (std::string(config.name) + ".a");
			auto dbc_output_path = artifacts_path.getPath() / (std::string(config.name) + ".dbc");

			assertTrue(
				std::filesystem::exists(a_output_path),
				base::strConcat(
					"Missing standard library static library artifact: ", a_output_path.string()
				)
			);
			assertTrue(
				std::filesystem::exists(dbc_output_path),
				base::strConcat("Missing standard library DVM artifact: ", dbc_output_path.string())
			);
		}
	}
};

TESTER_COMMON_MAIN("/src/compiler/driver/driver/tests/")
