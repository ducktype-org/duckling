// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include <driver/diagnostics/log_helpers.hpp>
#include <driver/manifest/manifest.hpp>
#include <driver/task/task.hpp>
#include <frontend/packages/packages.hpp>
#include <global_state/global_logger.hpp>

#include <base/pointers/box.hpp>
#include <base/pointers/ref.hpp>

#include <diagnostic/core/diagnostic_arguments.hpp>
#include <diagnostic/logger.hpp>
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
		TESTER_ADD_TEST(missingPackageFieldsFails);
		TESTER_ADD_TEST(missingTaskPackageFails);
		TESTER_ADD_TEST(unknownFieldsWarnNotError);
		TESTER_ADD_TEST(verifyEmptyPackagesFails);
		TESTER_ADD_TEST(verifyDuplicateAndUnknownDepsFail);
		TESTER_ADD_TEST(dvmStrategyParsed);
		TESTER_ADD_TEST(dvmStrategyLinkingOptionsParsed);
		TESTER_ADD_TEST(dvmLibStrategyLinkingOptionsParsed);
		TESTER_ADD_TEST(dvmStrategyBadLinkingOptionsFails);
		TESTER_ADD_TEST(nativeStrategyParsed);
		TESTER_ADD_TEST(nativeStrategyBadLinkingOptionsFails);
		TESTER_ADD_TEST(nativeStrategyLinkingOptionsObjectParsed);
		TESTER_ADD_TEST(libStrategyBadArchiveOptionsFails);
		TESTER_ADD_TEST(dvmStrategyMissingOutputFails);
		TESTER_ADD_TEST(unknownStrategyFails);
	}

protected:
	void beforeAll() override { global_state::setters::setGlobalLogger(makeBox<dia::Logger>()); }

private:
	dia::Logger& logger() { return *global_state::getGlobalLogger(); }

	void clearLogger() { logger().clear(); }

	bool hasWarning() { return logger().warningCount() > 0; }

	// ------------------------------------------------------------

	void validManifestParsed() {
		clearLogger();

		auto manifest_json = nlohmann::json::parse(R"({
            "packages": [
                {
                    "id": "mylib",
                    "name": "mylib",
                    "version": "1.0.0",
                    "path": "pkgs/mylib",
                    "features": ["feature_a"],
                    "dependencies": []
                },
                {
                    "id": "app",
                    "name": "app",
                    "version": "2.0.0",
                    "path": "pkgs/app",
                    "features": [],
                    "dependencies": [
                        { "id": "mylib", "alias": "mylib" }
                    ]
                }
            ],
            "tasks": [
                {
                    "package": "app",
                    "strategy": "dvm_exe",
                    "output_file": "bin/app_dvm"
                }
            ]
        })");

		auto result = PackageCompilationManifest::fromJson(
			manifest_json, diagnostics::makeGlobalLoggerReporter()
		);

		ASSERT_HAS_VALUE(result);
		ASSERT_TRUE(logger().good());
		std::ignore = result->verify(diagnostics::makeGlobalLoggerReporter());
		ASSERT_EQUAL(result->packages.size(), 2u);
		ASSERT_EQUAL(result->tasks.size(), 1u);
		ASSERT_EQUAL(result->packages[0].package_id.str(), std::string("mylib"));
		ASSERT_EQUAL(result->packages[0].package_name.str(), std::string("mylib"));
		ASSERT_EQUAL(result->packages[1].package_id.str(), std::string("app"));
		ASSERT_EQUAL(result->packages[1].package_name.str(), std::string("app"));
		ASSERT_EQUAL(result->packages[1].dependencies.size(), 1u);
		ASSERT_EQUAL(result->packages[1].dependencies[0].package_id.str(), std::string("mylib"));
	}

	void missingPackagesFieldFails() {
		clearLogger();

		auto manifest_json = nlohmann::json::parse(R"({
            "tasks": []
        })");

		auto result = PackageCompilationManifest::fromJson(
			manifest_json, diagnostics::makeGlobalLoggerReporter()
		);

		ASSERT_NO_VALUE(result);
		ASSERT_TRUE(logger().hasErrors());
	}

	void missingPackageFieldsFails() {
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

		ASSERT_NO_VALUE(result);
		ASSERT_TRUE(logger().hasErrors());
	}

	void missingTaskPackageFails() {
		clearLogger();

		auto manifest_json = nlohmann::json::parse(R"({
            "packages": [],
            "tasks": [
                { "strategy": "dvm_exe" }
            ]
        })");

		auto result = PackageCompilationManifest::fromJson(
			manifest_json, diagnostics::makeGlobalLoggerReporter()
		);

		ASSERT_NO_VALUE(result);
		ASSERT_TRUE(logger().hasErrors());
	}

	void unknownFieldsWarnNotError() {
		clearLogger();

		auto manifest_json = nlohmann::json::parse(R"({
            "packages": [
                {
                    "id": "app",
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
                    "strategy": "dvm_exe",
                    "output_file": "bin/app_dvm",
                    "name": "build_app"
                }
            ]
        })");

		auto result = PackageCompilationManifest::fromJson(
			manifest_json, diagnostics::makeGlobalLoggerReporter()
		);

		ASSERT_HAS_VALUE(result);
		ASSERT_TRUE(!logger().hasErrors());
		std::ignore = result->verify(diagnostics::makeGlobalLoggerReporter());
		ASSERT_TRUE(hasWarning());
	}

	void verifyEmptyPackagesFails() {
		clearLogger();

		PackageCompilationManifest manifest{
			.packages = {},
			.tasks    = {},
		};

		auto result = manifest.verify(diagnostics::makeGlobalLoggerReporter());
		ASSERT_TRUE(result.isBad());
		ASSERT_TRUE(logger().hasErrors());
	}

	void verifyDuplicateAndUnknownDepsFail() {
		clearLogger();

		PackageCompilationManifest manifest{
			.packages = {
				compiler::frontend::packages::RawPackageInfo{
					.package_id   = base::StrID("pkg"),
					.package_name = base::StrID("pkg"),
					.version      = base::StrID("1"),
					.package_path = fs::FilePath("/tmp/pkg"),
					.features     = {},
					.dependencies = {
						compiler::frontend::packages::RawDependencyInfo{
							.package_id = base::StrID("missing"),
							.alias      = base::StrID("dup"),
						},
						compiler::frontend::packages::RawDependencyInfo{
							.package_id = base::StrID("missing"),
							.alias      = base::StrID("dup"),
						},
					},
				},
				compiler::frontend::packages::RawPackageInfo{
					.package_id   = base::StrID("pkg"),
					.package_name = base::StrID("pkg"),
					.version      = base::StrID("1"),
					.package_path = fs::FilePath("/tmp/pkg2"),
					.features     = {},
					.dependencies = {},
				},
			},
			.tasks = {},
		};

		auto result = manifest.verify(diagnostics::makeGlobalLoggerReporter());
		ASSERT_TRUE(result.isBad());
		ASSERT_TRUE(logger().hasErrors());
	}

	void dvmStrategyParsed() {
		clearLogger();

		auto task_json = nlohmann::json::parse(
			R"({ "package": "mylib", "strategy": "dvm_lib", "output_file": "bin/mylib_dvm" })"
		);
		auto result = RawPackageCompilationTask::fromJson(
			task_json, diagnostics::makeGlobalLoggerReporter()
		);

		ASSERT_HAS_VALUE(result);
		ASSERT_TRUE(logger().good());
		ASSERT_MATCHES(result->build_target, BuildTargetDVMLibrary);
		const auto& target = std::get<BuildTargetDVMLibrary>(result->build_target);
		ASSERT_EQUAL(target.output_file_name.str(), std::string("bin/mylib_dvm"));
		ASSERT_EQUAL(result->package_id.str(), std::string("mylib"));
	}

	void dvmStrategyLinkingOptionsParsed() {
		clearLogger();

		auto task_json = nlohmann::json::parse(R"({
            "package": "app",
            "strategy": "dvm_exe",
            "output_file": "bin/app",
            "dvm_linking_options": {
                "shared_libraries": [ "libm.so.6", "libfoo.so" ],
                "link_libraries": [ "bin/mylib_dvm.dbc" ]
            }
        })");
		auto result    = RawPackageCompilationTask::fromJson(
            task_json, diagnostics::makeGlobalLoggerReporter()
        );

		ASSERT_HAS_VALUE(result);
		ASSERT_TRUE(logger().good());
		ASSERT_MATCHES(result->build_target, BuildTargetDVMExecutable);
		const auto& target = std::get<BuildTargetDVMExecutable>(result->build_target);
		ASSERT_EQUAL(
			(std::vector<std::string>{ "libm.so.6", "libfoo.so" }),
			target.dvm_linking_options.shared_libraries
		);
		ASSERT_EQUAL(
			(std::vector<fs::FilePath>{ fs::FilePath("bin/mylib_dvm.dbc") }),
			target.dvm_linking_options.link_libraries
		);
	}

	void dvmLibStrategyLinkingOptionsParsed() {
		clearLogger();

		auto task_json = nlohmann::json::parse(R"({
            "package": "mylib",
            "strategy": "dvm_lib",
            "output_file": "bin/mylib_dvm",
            "dvm_linking_options": { "shared_libraries": [ "libm.so.6" ] }
        })");
		auto result    = RawPackageCompilationTask::fromJson(
            task_json, diagnostics::makeGlobalLoggerReporter()
        );

		ASSERT_HAS_VALUE(result);
		ASSERT_TRUE(logger().good());
		ASSERT_MATCHES(result->build_target, BuildTargetDVMLibrary);
		const auto& target = std::get<BuildTargetDVMLibrary>(result->build_target);
		ASSERT_EQUAL(
			(std::vector<std::string>{ "libm.so.6" }), target.dvm_linking_options.shared_libraries
		);
		ASSERT_TRUE(target.dvm_linking_options.link_libraries.empty());
	}

	void dvmStrategyBadLinkingOptionsFails() {
		clearLogger();

		auto task_json = nlohmann::json::parse(R"({
            "package": "app",
            "strategy": "dvm_exe",
            "output_file": "bin/app",
            "dvm_linking_options": { "shared_libraries": [ 123 ] }
        })");
		auto result    = RawPackageCompilationTask::fromJson(
            task_json, diagnostics::makeGlobalLoggerReporter()
        );

		ASSERT_NO_VALUE(result);
		ASSERT_TRUE(logger().hasErrors());

		clearLogger();

		auto not_an_object_json   = nlohmann::json::parse(R"({
            "package": "app",
            "strategy": "dvm_exe",
            "output_file": "bin/app",
            "dvm_linking_options": "libm.so.6"
        })");
		auto not_an_object_result = RawPackageCompilationTask::fromJson(
			not_an_object_json, diagnostics::makeGlobalLoggerReporter()
		);

		ASSERT_NO_VALUE(not_an_object_result);
		ASSERT_TRUE(logger().hasErrors());
	}

	void nativeStrategyParsed() {
		clearLogger();

		auto task_json = nlohmann::json::parse(
			R"({ "package": "app", "strategy": "native", "output_file": "bin/app", "linking_options": "-lm" })"
		);
		auto result = RawPackageCompilationTask::fromJson(
			task_json, diagnostics::makeGlobalLoggerReporter()
		);

		ASSERT_HAS_VALUE(result);
		ASSERT_TRUE(logger().good());
		ASSERT_MATCHES(result->build_target, BuildTargetLLVMExecutable);
		const auto& target = std::get<BuildTargetLLVMExecutable>(result->build_target);
		ASSERT_EQUAL(target.linking_options.additional_link_options, std::string("-lm"));
	}

	void nativeStrategyBadLinkingOptionsFails() {
		clearLogger();

		auto task_json = nlohmann::json::parse(R"({
            "package": "app",
            "strategy": "native",
            "output_file": "bin/app",
            "linking_options": { "linker": 123 }
        })");
		auto result    = RawPackageCompilationTask::fromJson(
            task_json, diagnostics::makeGlobalLoggerReporter()
        );

		ASSERT_NO_VALUE(result);
		ASSERT_TRUE(logger().hasErrors());
	}

	void nativeStrategyLinkingOptionsObjectParsed() {
		clearLogger();

		auto task_json = nlohmann::json::parse(R"({
            "package": "app",
            "strategy": "native",
            "output_file": "bin/app",
            "linking_options": {
                "linker": "ld",
                "additional_link_options": "-lfoo",
                "link_c_standard_library": false
            }
        })");
		auto result    = RawPackageCompilationTask::fromJson(
            task_json, diagnostics::makeGlobalLoggerReporter()
        );

		ASSERT_HAS_VALUE(result);
		ASSERT_TRUE(logger().good());
		ASSERT_MATCHES(result->build_target, BuildTargetLLVMExecutable);
		const auto& target = std::get<BuildTargetLLVMExecutable>(result->build_target);
		ASSERT_EQUAL(target.linking_options.linker_path, std::string("ld"));
		ASSERT_EQUAL(target.linking_options.additional_link_options, std::string("-lfoo"));
		ASSERT_TRUE(!target.linking_options.link_c_standard_library);
	}

	void libStrategyBadArchiveOptionsFails() {
		clearLogger();

		auto task_json = nlohmann::json::parse(R"({
            "package": "lib",
            "strategy": "lib",
            "output_file": "bin/lib.a",
            "archive_options": { "archiver": 123 }
        })");
		auto result    = RawPackageCompilationTask::fromJson(
            task_json, diagnostics::makeGlobalLoggerReporter()
        );

		ASSERT_NO_VALUE(result);
		ASSERT_TRUE(logger().hasErrors());
	}

	void dvmStrategyMissingOutputFails() {
		clearLogger();

		auto task_json = nlohmann::json::parse(R"({
            "package": "app",
            "strategy": "dvm_exe"
        })");
		auto result    = RawPackageCompilationTask::fromJson(
            task_json, diagnostics::makeGlobalLoggerReporter()
        );

		ASSERT_NO_VALUE(result);
		ASSERT_TRUE(logger().hasErrors());
	}

	void unknownStrategyFails() {
		clearLogger();

		auto task_json = nlohmann::json::parse(R"({ "package": "app", "strategy": "wasm" })");
		auto result    = RawPackageCompilationTask::fromJson(
            task_json, diagnostics::makeGlobalLoggerReporter()
        );

		ASSERT_NO_VALUE(result);
		ASSERT_TRUE(logger().hasErrors());
	}

public:
	~ManifestJsonTest() override = default;
};

TESTER_COMMON_MAIN("/src/compiler/driver/driver/tests/");
