#include <diagnostic_interactive/logger.hpp>
#include <driver/diagnostics/log_helpers.hpp>
#include <driver/exit.hpp>
#include <driver/initialize.hpp>
#include <driver/manifest/manifest.hpp>
#include <driver/operations/generic_operations.hpp>
#include <driver/task/task.hpp>
#include <frontend/module_tree/functors.hpp>
#include <frontend/module_tree/module_tree.hpp>
#include <global_state/backend_options.hpp>
#include <global_state/global_logger.hpp>
#include <global_state/packages.hpp>

#include <base/extend_cpp/variant_match.hpp>
#include <base/str/str_utils.hpp>

#include <filesystem/file_path.hpp>
#include <json/json.hpp>
#include <tester/tester.hpp>

#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

using namespace compiler;

namespace {
	std::string rewriteBuildPaths(std::string_view options, const fs::FilePath& artifacts_path) {
		const std::string build_prefix = "build/";
		const std::string replacement = artifacts_path.getPath().string() + "/";
		std::string       result(options);
		size_t            pos = 0;
		while ((pos = result.find(build_prefix, pos)) != std::string::npos) {
			result.replace(pos, build_prefix.size(), replacement);
			pos += replacement.size();
		}
		return result;
	}
}

class PackagesTest final: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS PackagesTest

	fs::FilePath artifacts_path = fs::FileManager::createRandomTempDirectory().getFilePath();
	driver::PackageCompilationManifest manifest{};

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(packagesRegisteredInGlobalState);
		TESTER_ADD_TEST(compilePackagesFromManifest);
	}

protected:
	void beforeAll() override {
		global_state::setters::setGlobalLogger(makeBox<dia_int::Logger>());

		manifest = loadManifest();

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
			}
		);

		ASSERT_TRUE(init_result.status().isOk());
	}

	void afterAll() override {
		compiler::driver::exit();
		std::filesystem::remove_all(artifacts_path.getPath());
	}

private:
	driver::PackageCompilationManifest loadManifest() {
		const auto manifest_path = fs::FilePath(path("modules/packages/manifest.json"));
		std::ifstream in(manifest_path.getPath());
		assertTrue(in.is_open(), "Failed to open packages manifest for driver tests");

		nlohmann::json manifest_json = nlohmann::json::parse(in, nullptr, true, true);
		auto manifest_opt = driver::PackageCompilationManifest::fromJson(
			manifest_json, compiler::driver::diagnostics::makeGlobalLoggerReporter()
		);
		assertTrue(manifest_opt.has_value(), "Failed to parse packages manifest");
		assertTrue(
			manifest_opt->verify(compiler::driver::diagnostics::makeGlobalLoggerReporter()).isOk(),
			"Manifest verification failed"
		);

		for (auto& package_info: manifest_opt->packages) {
			package_info.package_path = fs::FilePath(
				path(base::strConcat("modules/packages/", package_info.package_name.str()))
			);
		}

		return std::move(*manifest_opt);
	}

	void packagesRegisteredInGlobalState() {
		const auto& packages = global_state::getPackages();
		ASSERT_EQUAL(packages.size(), manifest.packages.size());

		std::unordered_set<std::string> expected_names;
		for (const auto& pkg: manifest.packages)
			expected_names.insert(pkg.package_name.str());

		for (const auto& pkg: packages) {
			auto package_id = pkg.getPackageID().str();
			assertTrue(expected_names.contains(package_id), "Unexpected package in global state");

			auto root_id = pkg.getRootModule().illegalAccess().getID();
			auto root_ref = frontend::getModuleRef(root_id);
			ASSERT_EQUAL(root_ref->getName().str(), package_id);
		}

		auto get_dep_ids = [](const frontend::packages::PackageInfo& pkg) {
			std::unordered_set<std::string> deps;
			for (const auto& dep: pkg.getDependencies().illegalAccess())
				deps.insert(dep.getPackage().illegalAccess().getID().str());
			return deps;
		};

		std::unordered_map<std::string, std::unordered_set<std::string>> expected_deps{
			{ "lib_a", {} },
			{ "lib_b", { "lib_a" } },
			{ "app1", { "lib_a", "lib_b" } },
			{ "app2", { "lib_a" } },
		};

		for (const auto& pkg: packages) {
			auto deps = get_dep_ids(pkg);
			ASSERT_TRUE(expected_deps.contains(pkg.getPackageID().str()));
			ASSERT_TRUE(deps == expected_deps.at(pkg.getPackageID().str()));
		}
	}

	void compilePackagesFromManifest() {
		std::vector<driver::PackageCompilationTask> tasks;
		tasks.reserve(manifest.tasks.size());

		for (const auto& raw_task: manifest.tasks) {
			auto task_opt = driver::convertRawTaskToTask(
				raw_task, compiler::driver::diagnostics::makeGlobalLoggerReporter()
			);
			ASSERT_TRUE(task_opt.has_value());
			ASSERT_TRUE(task_opt->type == driver::TaskType::PackageCompilation);
			auto task_data = std::get<driver::PackageCompilationTask>(task_opt->task_data);
			tasks.push_back(std::move(task_data));
		}

		for (auto& task: tasks) {
			if (auto* target_exe = std::get_if<driver::BuildTargetLLVMExecutable>(
				&task.build_target
			)) {
				target_exe->linking_options.additional_link_options = rewriteBuildPaths(
					target_exe->linking_options.additional_link_options,
					artifacts_path
				);
			}
		}

		ASSERT_TRUE(driver::compilePackages(tasks).isOk());

		const std::vector<std::string> expected_outputs{
			"lib_a.a",
			"lib_b.a",
			"lib_a_dvm.dbc",
			"lib_b_dvm.dbc",
			"app1_dvm.dbc",
			"app2_dvm.dbc",
			"app1.exe",
			"app2.exe",
		};

		for (const auto& output: expected_outputs) {
			auto output_path = artifacts_path.getPath() / output;
			assertTrue(
				std::filesystem::exists(output_path),
				base::strConcat("Missing package artifact: ", output_path.string())
			);
		}
	}
};

TESTER_COMMON_MAIN("/src/compiler/driver/driver/tests/")
