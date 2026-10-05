// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include <driver/diagnostics/log_helpers.hpp>
#include <driver/exit.hpp>
#include <driver/initialize.hpp>
#include <driver/manifest/manifest.hpp>
#include <driver/operations/generic_operations.hpp>
#include <driver/standard_library/standard_library.hpp>
#include <driver/task/task.hpp>
#include <driver_private/standard_library/standard_library.hpp>
#include <frontend/packages/standard_packages.hpp>
#include <global_state/backend_options.hpp>
#include <global_state/global_logger.hpp>
#include <global_state/packages.hpp>

#include <base/extend_cpp/vector_utils.hpp>
#include <base/pointers/box.hpp>
#include <base/str/str_utils.hpp>

#include <diagnostic/logger.hpp>
#include <filesystem/file_path.hpp>
#include <os_utils/system_libraries.hpp>
#include <tester/tester.hpp>

#include <json/json.hpp>

#include <algorithm>
#include <fstream>
using namespace compiler;

class StdPackagesTest final: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS StdPackagesTest

	fs::FilePath artifacts_path = fs::FileManager::createRandomTempDirectory().getFilePath();
	driver::PackageCompilationManifest manifest{};

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(verifyStdPackagesAndDependencies);
		TESTER_ADD_TEST(compileStdPackages);
		// The ones below need the standard library artifacts to already exist, so they run after
		// compileStdPackages.
		TESTER_ADD_TEST(verifyStdLinkingOptionsInConvertedTasks);
		TESTER_ADD_TEST(verifyStdDVMLinkingOptionsInConvertedTasks);
		TESTER_ADD_TEST(verifyStdSharedLibsDeclaredForCore);
	}

protected:
	/**
	 * @brief Global setup executed once before all tests.
	 *
	 * - Initializes global logger
	 * - Initializes the compiler in package compilation mode
	 */
	void beforeAll() override {
		global_state::setters::setGlobalLogger(makeBox<dia::Logger>());
		const auto     manifest_path = fs::FilePath(path("modules/packages/manifest.json"));
		std::ifstream  in(manifest_path.getPath());
		nlohmann::json manifest_json = nlohmann::json::parse(in, nullptr, true, true);
		auto           manifest_opt  = driver::PackageCompilationManifest::fromJson(
            manifest_json, compiler::driver::diagnostics::makeGlobalLoggerReporter()
        );
		ASSERT_HAS_VALUE(manifest_opt, "Failed to parse packages manifest");
		assertTrue(
			manifest_opt->verify(compiler::driver::diagnostics::makeGlobalLoggerReporter()).isOk(),
			"Manifest verification failed"
		);
		manifest = std::move(*manifest_opt);


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
				.stdlib_options = { .std_lib_type = driver::options_types::StdLibOptions::DefaultStd{} },
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
		for (const auto& std_id: frontend::packages::standardLibraryPackageIds()) {
			if (std::ranges::none_of(global_packages, [&](const auto& pkg) {
					return pkg.getPackageID() == std_id;
				})) {
				assertTrue(false, base::strConcat("Standard library package not found: ", std_id));
			}
		}

		// Check that non-std packages have standard library dependencies
		for (const auto& pkg: global_packages) {
			// Skip checking dependencies of the std lib itself
			if (frontend::packages::isStandardLibraryPackage(pkg.getPackageID())) continue;

			// Verify it depends on all std packages
			for (const auto& std_id: frontend::packages::standardLibraryPackageIds()) {
				bool depends_on_std = false;
				for (const auto& dep: pkg.getDependencies().illegalAccess()) {
					if (dep.illegalAccess().getPackage().illegalAccess().getID() == std_id) {
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
		driver::options_types::StdLibOptions global_opts{
			.std_lib_type = driver::options_types::StdLibOptions::DefaultStd{}
		};

		for (const auto& raw_task: manifest.tasks) {
			auto task_opt = driver::convertRawTaskToTask(
				raw_task, global_opts, compiler::driver::diagnostics::makeGlobalLoggerReporter()
			);
			ASSERT_HAS_VALUE(task_opt);
			ASSERT_TRUE(task_opt->type == driver::TaskType::PackageCompilation);

			auto task_data = std::get<driver::PackageCompilationTask>(task_opt->task_data);

			if (auto* target_exe
			    = std::get_if<driver::BuildTargetLLVMExecutable>(&task_data.build_target)) {
				// We expect the correct stdlib linking options based on getStdLibLinkingArgs
				[[maybe_unused]] auto lib_args = driver::getNativeStdLibLinkingArgs(global_opts);

				// Ensure the options contain the stdlib args
				assertTrue(
					target_exe->linking_options.stdlib_link_options
						== driver::getNativeStdLibLinkingArgs(global_opts),
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

		for (const auto& art: driver::getStdLibDVMArtifacts()) {
			assertTrue(
				std::filesystem::exists(art.file.getFilePath().getPath()),
				base::strConcat(
					"Missing standard library DVM artifact: ",
					art.file.getFilePath().getPath().string()
				)
			);
		}
	}

	/**
	 * @brief Verifies that the standard library is linked into DVM executables through the
	 * `link_libraries` of their linking options, and that DVM libraries are left alone.
	 */
	void verifyStdDVMLinkingOptionsInConvertedTasks() {
		driver::options_types::StdLibOptions global_opts{
			.std_lib_type = driver::options_types::StdLibOptions::DefaultStd{}
		};

		auto std_link_libraries = driver::getStdLibDVMLinkingDependencies(global_opts);
		assertTrue(!std_link_libraries.empty(), "No standard library DVM artifacts to link");

		bool seen_executable = false;
		bool seen_library    = false;
		for (const auto& raw_task: manifest.tasks) {
			auto task_opt = driver::convertRawTaskToTask(
				raw_task, global_opts, compiler::driver::diagnostics::makeGlobalLoggerReporter()
			);
			ASSERT_HAS_VALUE(task_opt);
			auto task_data = std::get<driver::PackageCompilationTask>(task_opt->task_data);

			if (auto* target_exe
			    = std::get_if<driver::BuildTargetDVMExecutable>(&task_data.build_target)) {
				seen_executable = true;
				assertTrue(
					base::containsAllOf(
						target_exe->dvm_linking_options.link_libraries, std_link_libraries
					),
					"Converted DVM executable task does not link the standard library"
				);
			}

			if (auto* target_lib
			    = std::get_if<driver::BuildTargetDVMLibrary>(&task_data.build_target)) {
				seen_library = true;
				assertTrue(
					target_lib->dvm_linking_options.link_libraries.empty(),
					"Converted DVM library task should not link the standard library"
				);
			}
		}

		assertTrue(seen_executable, "The manifest has no DVM executable task to verify");
		assertTrue(seen_library, "The manifest has no DVM library task to verify");
	}

	/**
	 * @brief Verifies that `core` declares the system libraries providing its C standard library
	 * FFI symbols, and that they reach its DVM compilation task.
	 */
	void verifyStdSharedLibsDeclaredForCore() {
		const std::vector<std::string> expected_libs{ os_utils::systemSharedLibC(),
			                                          os_utils::systemSharedLibM() };

		auto core = std::ranges::find_if(
			frontend::packages::standardLibraryPackages(),
			[](const auto& package) { return package.id == base::StrID("core"); }
		);
		assertTrue(
			core != frontend::packages::standardLibraryPackages().end(),
			"No `core` standard library package"
		);
		ASSERT_EQUAL(expected_libs, core->getSharedLibsAsStr());

		bool seen_core_dvm_task = false;
		for (const auto& task: driver::getRequiredStdLibCompilationTasks()) {
			auto* target_lib = std::get_if<driver::BuildTargetDVMLibrary>(&task.build_target);
			if (target_lib == nullptr or target_lib->output_file_name != base::StrID("core.dbc"))
				continue;

			seen_core_dvm_task = true;
			ASSERT_EQUAL(expected_libs, target_lib->dvm_linking_options.shared_libraries);
		}
		assertTrue(seen_core_dvm_task, "No DVM compilation task for `core`");
	}
};

TESTER_COMMON_MAIN("/src/compiler/driver/driver/tests/")
