#include <frontend/packages/packages.hpp>

#include <base/pointers/box.hpp>
#include <base/str/str_utils.hpp>

#include <filesystem/file.hpp>
#include <tester/tester.hpp>

#include <json/json.hpp>

using namespace compiler::frontend::packages;

namespace {
	struct TestReporter {
		int errors   = 0;
		int warnings = 0;

		DiagnosticReporter callback() {
			return [this](std::string_view, std::string_view, bool is_error) {
				if (is_error)
					++errors;
				else
					++warnings;
			};
		}
	};
}

class PackagesTest final: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS PackagesTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(parseDependencySuccess);
		TESTER_ADD_TEST(parseDependencyMissingNameFails);
		TESTER_ADD_TEST(parsePackageSuccess);
		TESTER_ADD_TEST(parsePackageUnknownFieldsWarn);
		TESTER_ADD_TEST(parsePackageBadFeaturesFails);
		TESTER_ADD_TEST(createPackageInfoSuccess);
		TESTER_ADD_TEST(createPackageInfoMissingMainFails);
	}

private:
	void parseDependencySuccess() {
		TestReporter reporter;
		auto         json = nlohmann::json::parse(R"({ "name": "dep", "alias": "alias" })");
		auto         dep  = RawDependencyInfo::fromJson(json, reporter.callback());
		ASSERT_TRUE(dep.has_value());
		ASSERT_EQUAL(dep->package_name.str(), std::string("dep"));
		ASSERT_EQUAL(dep->alias.str(), std::string("alias"));
		ASSERT_EQUAL(reporter.errors, 0);
	}

	void parseDependencyMissingNameFails() {
		TestReporter reporter;
		auto         json = nlohmann::json::parse(R"({ "alias": "alias" })");
		auto         dep  = RawDependencyInfo::fromJson(json, reporter.callback());
		ASSERT_TRUE(!dep.has_value());
		ASSERT_TRUE(reporter.errors > 0);
	}

	void parsePackageSuccess() {
		TestReporter reporter;
		auto         json = nlohmann::json::parse(R"({
            "name": "pkg",
            "path": "VFS:/packages/pkg.dmf",
            "version": "1.2.3",
            "features": ["f1", "f2"],
            "dependencies": [
                { "name": "dep" }
            ]
        })");
		auto         pkg  = RawPackageInfo::fromJson(json, reporter.callback());
		ASSERT_TRUE(pkg.has_value());
		ASSERT_EQUAL(pkg->package_name.str(), std::string("pkg"));
		ASSERT_EQUAL(pkg->version.str(), std::string("1.2.3"));
		ASSERT_EQUAL(pkg->features.size(), 2u);
		ASSERT_EQUAL(pkg->dependencies.size(), 1u);
		ASSERT_EQUAL(pkg->dependencies[0].package_name.str(), std::string("dep"));
		ASSERT_EQUAL(reporter.errors, 0);
	}

	void parsePackageUnknownFieldsWarn() {
		TestReporter reporter;
		auto         json = nlohmann::json::parse(R"({
            "name": "pkg",
            "path": "VFS:/packages/pkg.dmf",
            "weird": 123,
            "dependencies": []
        })");
		auto         pkg  = RawPackageInfo::fromJson(json, reporter.callback());
		ASSERT_TRUE(pkg.has_value());
		ASSERT_EQUAL(reporter.errors, 0);
		ASSERT_TRUE(reporter.warnings > 0);
	}

	void parsePackageBadFeaturesFails() {
		TestReporter reporter;
		auto         json = nlohmann::json::parse(R"({
            "name": "pkg",
            "path": "VFS:/packages/pkg.dmf",
            "features": ["ok", 123],
            "dependencies": []
        })");
		auto         pkg  = RawPackageInfo::fromJson(json, reporter.callback());
		ASSERT_TRUE(!pkg.has_value());
		ASSERT_TRUE(reporter.errors > 0);
	}

	void createPackageInfoSuccess() {
		TestReporter   reporter;
		auto           main_file = fs::FileManager::createRandomVirtualFile("fn main() {}", ".dmf");
		RawPackageInfo raw{
			.package_name = base::StrID("pkg"),
			.version      = base::StrID("1.0.0"),
			.package_path = main_file.getFilePath(),
			.features     = { base::StrID("f1") },
			.dependencies
			= { RawDependencyInfo{ .package_name = base::StrID("dep"), .alias = base::StrID() } },
		};

		auto pkg_info = createPackageInfo(raw, reporter.callback());
		ASSERT_TRUE(pkg_info.has_value());
		auto deps = pkg_info->getDependencies().illegalAccess();
		ASSERT_EQUAL(deps.size(), 1u);
		ASSERT_EQUAL(deps[0].getAlias().str(), std::string("dep"));
		ASSERT_EQUAL(reporter.errors, 0);

		fs::FileManager::deleteFile(main_file);
	}

	void createPackageInfoMissingMainFails() {
		TestReporter   reporter;
		auto           empty_dir = fs::FileManager::createRandomVirtualDirectory();
		RawPackageInfo raw{
			.package_name = base::StrID("pkg"),
			.version      = base::StrID("1.0.0"),
			.package_path = empty_dir.getFilePath(),
			.features     = {},
			.dependencies = {},
		};

		auto pkg_info = createPackageInfo(raw, reporter.callback());
		ASSERT_TRUE(!pkg_info.has_value());
		ASSERT_TRUE(reporter.errors > 0);

		fs::FileManager::deleteFolder(empty_dir, true);
	}

public:
	~PackagesTest() override = default;
};

TESTER_COMMON_MAIN("/src/compiler/core/frontend/packages/tests/");
