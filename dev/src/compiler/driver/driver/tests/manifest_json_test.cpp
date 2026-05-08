#include <diagnostic_interactive/core/diagnostic_arguments.hpp>
#include <diagnostic_interactive/logger.hpp>
#include <driver/diagnostics/log_helpers.hpp>
#include <driver/manifest/manifest.hpp>
#include <driver/task/task.hpp>
#include <frontend/packages/packages.hpp>
#include <global_state/global_logger.hpp>

#include <base/pointers/box.hpp>
#include <base/pointers/ref.hpp>

#include <tester/tester.hpp>

#include <json/json.hpp>

using namespace compiler::driver;

class ManifestJsonTest final: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS ManifestJsonTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(validManifestParsed);
		TESTER_ADD_TEST(missingPackagesFieldFails);
		TESTER_ADD_TEST(missingPackageNameFails);
		TESTER_ADD_TEST(missingTaskPackageFails);
		TESTER_ADD_TEST(unknownFieldsWarnNotError);
		TESTER_ADD_TEST(dvmStrategyParsed);
		TESTER_ADD_TEST(nativeStrategyParsed);
		TESTER_ADD_TEST(unknownStrategyFails);
	}

protected:
	void beforeAll() override {
		global_state::setters::setGlobalLogger(makeBox<dia_int::Logger>());
	}

private:
	dia_int::Logger& logger() { return *global_state::getGlobalLogger(); }

	void clearLogger() { logger().clear(); }

	bool hasWarning() {
		std::vector<base::CRef<dia_int::dia_args::Diagnostic>> diags;
		logger().collectDiagnostics(diags);
		for (const auto& d: diags)
			if (d->main_message.metadata.type == "warning") return true;
		return false;
	}

	// ------------------------------------------------------------

	void validManifestParsed() {
		clearLogger();

		auto manifest_json = nlohmann::json::parse(R"({
            "packages": [
                {
                    "name": "mylib",
                    "version": "1.0.0",
                    "path": "pkgs/mylib",
                    "features": ["feature_a"],
                    "dependencies": []
                },
                {
                    "name": "app",
                    "version": "2.0.0",
                    "path": "pkgs/app",
                    "features": [],
                    "dependencies": [
                        { "name": "mylib", "alias": "mylib" }
                    ]
                }
            ],
            "tasks": [
                {
                    "package": "app",
                    "strategy": "dvm"
                }
            ]
        })");

		auto result = PackageCompilationManifest::fromJson(
			manifest_json, diagnostics::makeGlobalLoggerReporter()
		);

		ASSERT_TRUE(result.has_value());
		ASSERT_TRUE(logger().good());
		(void) result->verify(diagnostics::makeGlobalLoggerReporter());
		ASSERT_EQUAL(result->packages.size(), 2u);
		ASSERT_EQUAL(result->tasks.size(), 1u);
		ASSERT_EQUAL(result->packages[0].package_name.str(), std::string("mylib"));
		ASSERT_EQUAL(result->packages[1].package_name.str(), std::string("app"));
		ASSERT_EQUAL(result->packages[1].dependencies.size(), 1u);
		ASSERT_EQUAL(result->packages[1].dependencies[0].package_name.str(), std::string("mylib"));
	}

	void missingPackagesFieldFails() {
		clearLogger();

		auto manifest_json = nlohmann::json::parse(R"({
            "tasks": []
        })");

		auto result = PackageCompilationManifest::fromJson(
			manifest_json, diagnostics::makeGlobalLoggerReporter()
		);

		ASSERT_TRUE(!result.has_value());
		ASSERT_TRUE(logger().hasErrors());
	}

	void missingPackageNameFails() {
		clearLogger();

		auto manifest_json = nlohmann::json::parse(R"({
            "packages": [
                {
                    "version": "1.0.0",
                    "path": "pkgs/app",
                    "features": [],
                    "dependencies": []
                }
            ],
            "tasks": []
        })");

		auto result = PackageCompilationManifest::fromJson(
			manifest_json, diagnostics::makeGlobalLoggerReporter()
		);

		ASSERT_TRUE(!result.has_value());
		ASSERT_TRUE(logger().hasErrors());
	}

	void missingTaskPackageFails() {
		clearLogger();

		auto manifest_json = nlohmann::json::parse(R"({
            "packages": [],
            "tasks": [
                { "strategy": "dvm" }
            ]
        })");

		auto result = PackageCompilationManifest::fromJson(
			manifest_json, diagnostics::makeGlobalLoggerReporter()
		);

		ASSERT_TRUE(!result.has_value());
		ASSERT_TRUE(logger().hasErrors());
	}

	void unknownFieldsWarnNotError() {
		clearLogger();

		auto manifest_json = nlohmann::json::parse(R"({
            "packages": [
                {
                    "name": "app",
                    "version": "1.0.0",
                    "path": "pkgs/app",
                    "features": [],
                    "dependencies": [],
                    "main": "index.js",
                    "compilation_strategy": "precompiled"
                }
            ],
            "tasks": [
                {
                    "package": "app",
                    "strategy": "dvm",
                    "name": "build_app"
                }
            ]
        })");

		auto result = PackageCompilationManifest::fromJson(
			manifest_json, diagnostics::makeGlobalLoggerReporter()
		);

		ASSERT_TRUE(result.has_value());
		ASSERT_TRUE(!logger().hasErrors());
		(void) result->verify(diagnostics::makeGlobalLoggerReporter());
		ASSERT_TRUE(hasWarning());
	}

	void dvmStrategyParsed() {
		clearLogger();

		auto task_json = nlohmann::json::parse(
			R"({ "package": "mylib", "strategy": "dvm", "output_file": "bin/mylib_dvm" })"
		);
		auto result = RawPackageCompilationTask::fromJson(
			task_json, diagnostics::makeGlobalLoggerReporter()
		);

		ASSERT_TRUE(result.has_value());
		ASSERT_TRUE(logger().good());
		ASSERT_TRUE(std::holds_alternative<BuildTargetDVM>(result->build_target));
		const auto& target = std::get<BuildTargetDVM>(result->build_target);
		ASSERT_EQUAL(target.output_file_stem.str(), std::string("bin/mylib_dvm"));
		ASSERT_EQUAL(result->package_name.str(), std::string("mylib"));
	}

	void nativeStrategyParsed() {
		clearLogger();

		auto task_json = nlohmann::json::parse(
			R"({ "package": "app", "strategy": "native", "output_file": "bin/app", "linking_options": "-lm" })"
		);
		auto result = RawPackageCompilationTask::fromJson(
			task_json, diagnostics::makeGlobalLoggerReporter()
		);

		ASSERT_TRUE(result.has_value());
		ASSERT_TRUE(logger().good());
		ASSERT_TRUE(std::holds_alternative<BuildTargetLLVMExecutable>(result->build_target));
		const auto& target = std::get<BuildTargetLLVMExecutable>(result->build_target);
		ASSERT_EQUAL(target.linking_options.additional_link_options, std::string("-lm"));
	}

	void unknownStrategyFails() {
		clearLogger();

		auto task_json = nlohmann::json::parse(R"({ "package": "app", "strategy": "wasm" })");
		auto result    = RawPackageCompilationTask::fromJson(
            task_json, diagnostics::makeGlobalLoggerReporter()
        );

		ASSERT_TRUE(!result.has_value());
		ASSERT_TRUE(logger().hasErrors());
	}

public:
	~ManifestJsonTest() override = default;
};

TESTER_COMMON_MAIN("/src/compiler/driver/driver/tests/");
