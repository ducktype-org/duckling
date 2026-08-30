#include <frontend/packages/packages.hpp>

#include <base/pointers/box.hpp>
#include <base/str/str_utils.hpp>

#include <filesystem/file.hpp>
#include <hashing/add_to_hash.hpp>
#include <hashing/component_hash.hpp>
#include <query_framework/entry/with_context_do.hpp>
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
		TESTER_ADD_TEST(parseDependencyAliasWrongTypeFails);
		TESTER_ADD_TEST(parsePackageSuccess);
		TESTER_ADD_TEST(parsePackageUnknownFieldsWarn);
		TESTER_ADD_TEST(parsePackageBadFeaturesFails);
		TESTER_ADD_TEST(parsePackageVersionWrongTypeFails);
		TESTER_ADD_TEST(parsePackageDependenciesNotArrayFails);
		TESTER_ADD_TEST(packageSideInputKeysRoundTrip);
		TESTER_ADD_TEST(packageDependenciesAccessUnlock);
		TESTER_ADD_TEST(filterUndeclaredDependenciesDropsMissing);
		TESTER_ADD_TEST(createPackageInfoSuccess);
		TESTER_ADD_TEST(createPackageInfoMissingMainFails);
	}

private:
	void parseDependencySuccess() {
		TestReporter reporter;
		auto         json = nlohmann::json::parse(R"({ "id": "dep", "alias": "alias" })");
		auto         dep  = RawDependencyInfo::fromJson(json, reporter.callback());
		ASSERT_HAS_VALUE(dep);
		ASSERT_EQUAL(dep->package_id.str(), std::string("dep"));
		ASSERT_HAS_VALUE(dep->alias);
		ASSERT_EQUAL(dep->alias.value().str(), std::string("alias"));
		ASSERT_EQUAL(reporter.errors, 0);
	}

	void parseDependencyMissingNameFails() {
		TestReporter reporter;
		auto         json = nlohmann::json::parse(R"({ "alias": "alias" })");
		auto         dep  = RawDependencyInfo::fromJson(json, reporter.callback());
		ASSERT_NO_VALUE(dep);
		ASSERT_TRUE(reporter.errors > 0);
	}

	void parseDependencyAliasWrongTypeFails() {
		TestReporter reporter;
		auto         json = nlohmann::json::parse(R"({ "id": "dep", "alias": 123 })");
		auto         dep  = RawDependencyInfo::fromJson(json, reporter.callback());
		ASSERT_NO_VALUE(dep);
		ASSERT_TRUE(reporter.errors > 0);
	}

	void parsePackageSuccess() {
		TestReporter reporter;
		auto         json = nlohmann::json::parse(R"({
            "id": "pkg_id",
            "name": "pkg",
            "path": "VFS:/packages/pkg.dk",
            "version": "1.2.3",
            "features": ["f1", "f2"],
            "dependencies": [
                { "id": "dep" }
            ]
        })");
		auto         pkg  = RawPackageInfo::fromJson(json, reporter.callback());
		ASSERT_HAS_VALUE(pkg);
		ASSERT_EQUAL(pkg->package_id.str(), std::string("pkg_id"));
		ASSERT_EQUAL(pkg->package_name.str(), std::string("pkg"));
		ASSERT_EQUAL(pkg->version.str(), std::string("1.2.3"));
		ASSERT_EQUAL(pkg->features.size(), 2u);
		ASSERT_EQUAL(pkg->dependencies.size(), 1u);
		ASSERT_EQUAL(pkg->dependencies[0].package_id.str(), std::string("dep"));
		ASSERT_EQUAL(reporter.errors, 0);
	}

	void parsePackageUnknownFieldsWarn() {
		TestReporter reporter;
		auto         json = nlohmann::json::parse(R"({
            "id": "pkg",
            "name": "pkg",
            "path": "VFS:/packages/pkg.dk",
            "weird": 123,
            "dependencies": []
        })");
		auto         pkg  = RawPackageInfo::fromJson(json, reporter.callback());
		ASSERT_HAS_VALUE(pkg);
		ASSERT_EQUAL(reporter.errors, 0);
		ASSERT_TRUE(reporter.warnings > 0);
	}

	void parsePackageBadFeaturesFails() {
		TestReporter reporter;
		auto         json = nlohmann::json::parse(R"({
            "id": "pkg",
            "name": "pkg",
            "path": "VFS:/packages/pkg.dk",
            "features": ["ok", 123],
            "dependencies": []
        })");
		auto         pkg  = RawPackageInfo::fromJson(json, reporter.callback());
		ASSERT_NO_VALUE(pkg);
		ASSERT_TRUE(reporter.errors > 0);
	}

	void parsePackageVersionWrongTypeFails() {
		TestReporter reporter;
		auto         json = nlohmann::json::parse(R"({
            "id": "pkg",
            "name": "pkg",
            "path": "VFS:/packages/pkg.dk",
            "version": 123,
            "dependencies": []
        })");
		auto         pkg  = RawPackageInfo::fromJson(json, reporter.callback());
		ASSERT_NO_VALUE(pkg);
		ASSERT_TRUE(reporter.errors > 0);
	}

	void parsePackageDependenciesNotArrayFails() {
		TestReporter reporter;
		auto         json = nlohmann::json::parse(R"({
            "id": "pkg",
            "name": "pkg",
            "path": "VFS:/packages/pkg.dk",
            "dependencies": { "id": "dep" }
        })");
		auto         pkg  = RawPackageInfo::fromJson(json, reporter.callback());
		ASSERT_NO_VALUE(pkg);
		ASSERT_TRUE(reporter.errors > 0);
	}

	void packageSideInputKeysRoundTrip() {
		hashing::ComponentHash::HashAlg hasher;
		hashing::addToHash(hasher, base::StrID("pkg"));
		auto pkg_hash = hasher.finalize();

		KeyOf_PackageSideInput pkg_key{ pkg_hash };
		std::ignore = pkg_key.queryStablePerfectHash();

		auto count_key = KeyOf_PackageDependencyCountSideInput::computeHash(base::StrID("pkg"), 3u);
		std::ignore    = count_key.queryStablePerfectHash();

		KeyOf_PackageDependencyAliasSideInput alias_key{
			.package_hash      = pkg_hash,
			.alias             = base::StrID("alias"),
			.found             = true,
			.target_package_id = base::StrID("dep"),
		};
		std::ignore = alias_key.queryStablePerfectHash();

		auto bytes   = alias_key.serialize();
		auto decoded = KeyOf_PackageDependencyAliasSideInput::deserialize(bytes);
		ASSERT_TRUE(decoded == alias_key);

		std::ostringstream os;
		alias_key.prettyPrint(os);
		ASSERT_TRUE(!os.str().empty());

		KeyOf_PackageDependencyAliasSideInput missing_key{
			.package_hash      = pkg_hash,
			.alias             = base::StrID("missing"),
			.found             = false,
			.target_package_id = {},
		};
		auto missing_bytes   = missing_key.serialize();
		auto missing_decoded = KeyOf_PackageDependencyAliasSideInput::deserialize(missing_bytes);
		ASSERT_TRUE(missing_decoded == missing_key);
	}

	void packageDependenciesAccessUnlock() {
		TestReporter reporter;
		auto         main_file = fs::FileManager::createRandomVirtualFile("fn main() {}", ".dk");

		RawPackageInfo raw{
			.package_id   = base::StrID("pkg"),
			.package_name = base::StrID("pkg"),
			.version      = base::StrID("1.0.0"),
			.package_path = main_file.getFilePath(),
			.features     = {},
			.dependencies = {
				RawDependencyInfo{ .package_id = base::StrID("dep"), .alias = {} },
				RawDependencyInfo{ .package_id = base::StrID("lib"), .alias = base::StrID("l") },
			},
		};
		RawPackageInfo dep{
			.package_id   = base::StrID("dep"),
			.package_name = base::StrID("dep"),
			.version      = base::StrID("1.0.0"),
			.package_path = main_file.getFilePath(),
			.features     = {},
			.dependencies = {},
		};
		RawPackageInfo lib{
			.package_id   = base::StrID("lib"),
			.package_name = base::StrID("lib"),
			.version      = base::StrID("1.0.0"),
			.package_path = main_file.getFilePath(),
			.features     = {},
			.dependencies = {},
		};


		auto pkg_info = createPackageInfo(raw, { raw, dep, lib }, reporter.callback());
		ASSERT_HAS_VALUE(pkg_info);
		ASSERT_EQUAL(reporter.errors, 0);

		query::utils::withContextDo([&](query::Context& ctx) {
			auto deps = pkg_info->getDependencies().unlock(ctx);
			ASSERT_EQUAL(deps.size(), 2u);

			auto dep_pkg = deps[0].unlock(ctx).getPackage().unlock(ctx);
			ASSERT_EQUAL(dep_pkg.getID().str(), std::string("dep"));

			auto alias_dep = pkg_info->getPackageDependencyByAlias(base::StrID("l")).unlock(ctx);
			ASSERT_HAS_VALUE(alias_dep);
			ASSERT_EQUAL(alias_dep.value().unlock(ctx).getID().str(), std::string("lib"));

			auto missing
				= pkg_info->getPackageDependencyByAlias(base::StrID("missing")).unlock(ctx);
			ASSERT_NO_VALUE(missing);
		});

		fs::FileManager::deleteFile(main_file);
	}

	void filterUndeclaredDependenciesDropsMissing() {
		TestReporter                reporter;
		std::vector<RawPackageInfo> packages;
		packages.push_back(RawPackageInfo{
			.package_id   = base::StrID("a"),
			.package_name = base::StrID("a"),
			.version      = base::StrID("1"),
			.package_path = fs::FilePath("VFS:/packages/a"),
			.features     = {},
			.dependencies = {
				RawDependencyInfo{ .package_id = base::StrID("b"), .alias = {} },
				RawDependencyInfo{ .package_id = base::StrID("missing"), .alias = {} },
			},
		});
		packages.push_back(RawPackageInfo{
			.package_id   = base::StrID("b"),
			.package_name = base::StrID("b"),
			.version      = base::StrID("1"),
			.package_path = fs::FilePath("VFS:/packages/b"),
			.features     = {},
			.dependencies = {},
		});

		filterUndeclaredDependencies(packages, reporter.callback());
		ASSERT_EQUAL(packages[0].dependencies.size(), 1);
		ASSERT_EQUAL(packages[0].dependencies[0].package_id.str(), std::string("b"));
		ASSERT_TRUE(reporter.errors > 0);
	}

	void createPackageInfoSuccess() {
		TestReporter   reporter;
		auto           main_file = fs::FileManager::createRandomVirtualFile("fn main() {}", ".dk");
		RawPackageInfo raw{
			.package_id   = base::StrID("pkg"),
			.package_name = base::StrID("pkg"),
			.version      = base::StrID("1.0.0"),
			.package_path = main_file.getFilePath(),
			.features     = { base::StrID("f1") },
			.dependencies = { RawDependencyInfo{ .package_id = base::StrID("dep"), .alias = {} } },
		};
		RawPackageInfo dep{
			.package_id   = base::StrID("dep"),
			.package_name = base::StrID("dep"),
			.version      = base::StrID("1.0.0"),
			.package_path = main_file.getFilePath(),
			.features     = {},
			.dependencies = {},
		};
		auto pkg_info = createPackageInfo(raw, { raw, dep }, reporter.callback());
		ASSERT_HAS_VALUE(pkg_info);
		ASSERT_EQUAL(pkg_info->getVersion().str(), std::string("1.0.0"));
		ASSERT_EQUAL(pkg_info->getFeatures()->size(), 1);
		auto deps = pkg_info->getDependencies().illegalAccess();
		ASSERT_EQUAL(deps.size(), 1);
		ASSERT_EQUAL(deps[0].illegalAccess().getAlias().str(), std::string("dep"));
		ASSERT_EQUAL(reporter.errors, 0);

		fs::FileManager::deleteFile(main_file);
	}

	void createPackageInfoMissingMainFails() {
		TestReporter   reporter;
		auto           empty_dir = fs::FileManager::createRandomVirtualDirectory();
		RawPackageInfo raw{
			.package_id   = base::StrID("pkg"),
			.package_name = base::StrID("pkg"),
			.version      = base::StrID("1.0.0"),
			.package_path = empty_dir.getFilePath(),
			.features     = {},
			.dependencies = {},
		};

		auto pkg_info = createPackageInfo(raw, { raw }, reporter.callback());
		ASSERT_NO_VALUE(pkg_info);
		ASSERT_TRUE(reporter.errors > 0);

		fs::FileManager::deleteFolder(empty_dir, true);
	}

public:
	~PackagesTest() override = default;
};

TESTER_COMMON_MAIN("/src/compiler/core/frontend/packages/tests/");
