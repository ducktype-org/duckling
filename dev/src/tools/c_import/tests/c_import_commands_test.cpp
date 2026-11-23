// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include <c_import/commands.hpp>
#include <c_import/package.hpp>
#include <c_import/process.hpp>
#include <c_import/recipe.hpp>

#include <os_utils/thread_safe_wrappers.hpp>
#include <tester/tester.hpp>

#include <filesystem>
#include <format>
#include <iterator>
#include <string>

using namespace c_import;

namespace {

	namespace fs = std::filesystem;

	/// `bindings` without its `# Target:` line, which depends on the machine.
	std::string withoutTarget(const std::string& bindings) {
		std::string result;
		std::size_t start = 0;
		while (start < bindings.size()) {
			auto end  = bindings.find('\n', start);
			end       = end == std::string::npos ? bindings.size() : end + 1;
			auto line = bindings.substr(start, end - start);
			if (!line.starts_with("# Target: ")) result += line;
			start = end;
		}
		return result;
	}

	std::string contents(const fs::path& file) { return readFile(file).value_or("<missing>"); }

	/// A script standing in for `duck`.
	fs::path fakeDuck(const fs::path& dir, const std::string& body) {
		auto path = dir / "duck";
		(void) writeFile(path, "#!/bin/sh\n" + body + "\n");
		fs::permissions(path, fs::perms::owner_all, fs::perm_options::add);
		return path;
	}

}

/// The commands of `duckbind`, run on the fixture of its integration test.
class CImportCommandsTests final: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS CImportCommandsTests

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(newWritesThePackage);
		TESTER_ADD_TEST(regenIsDeterministicAndKeepsTheRootModule);
		TESTER_ADD_TEST(checkFindsEveryDifference);
		TESTER_ADD_TEST(bindingsWritesOnlyTheModules);
		TESTER_ADD_TEST(badInputIsReported);
		TESTER_ADD_TEST(verificationRunsDuck);
		TESTER_ADD_TEST(unsupportedDeclarationsAreSkippedWithReasons);
		TESTER_ADD_TEST(pkgConfigPackagesAreResolved);
		TESTER_ADD_TEST(manifestsThatCannotBeUpdatedAreReported);
	}

private:
	TempDir         dir;
	const fs::path& work = dir.path();

	/// A fresh `sl` package generated from the fixture header, vendored inside it.
	fs::path newFixturePackage(const std::string& name) {
		auto root   = work / name;
		auto header = root / "include" / "sdl_like.h";
		fs::create_directories(header.parent_path());
		fs::copy_file(path("sdl_like.h"), header, fs::copy_options::overwrite_existing);
		NewOptions options;
		options.at               = root;
		options.name             = "sl";
		options.recipe.headers   = { header.string() };
		options.recipe.cflags    = { "-I" + header.parent_path().string() };
		options.recipe.include   = { "SL_*", "match" };
		options.recipe.libraries = { "-lm" };
		options.verify           = false;
		assertEqual(0, newPackage(options), "`new` succeeds");
		return root;
	}

	void newWritesThePackage() {
		auto       root      = newFixturePackage("new");
		const auto generated = root / "src" / "generated";
		assertEqual(
			contents(path("expected_bindings.txt")),
			withoutTarget(contents(generated / "sl.dk")),
			"the bindings are the expected ones"
		);
		assertTrue(fs::exists(generated / "generated.dk"), "the generated root module");
		assertTrue(fs::exists(root / "src" / "src.dk"), "the user's root module");
		assertTrue(
			contents(root / RECIPE_FILE).find("\"include/sdl_like.h\"") != std::string::npos,
			"the recipe's header is relative to the package"
		);
		assertTrue(
			contents(root / "quackconfig.yaml").find("  external-libs: [m]") != std::string::npos,
			"the manifest names the library"
		);
	}

	void regenIsDeterministicAndKeepsTheRootModule() {
		auto       root     = newFixturePackage("regen");
		const auto bindings = root / "src" / "generated" / "sl.dk";
		const auto before   = contents(bindings);
		const auto user     = contents(root / "src" / "src.dk") + "# my helpers\n";
		(void) writeFile(root / "src" / "src.dk", user);
		assertEqual(0, regenPackage(root, false, "duck"), "`regen` succeeds");
		assertEqual(before, contents(bindings), "the bindings are the same");
		assertEqual(user, contents(root / "src" / "src.dk"), "`src.dk` is kept");
	}

	void checkFindsEveryDifference() {
		auto       root      = newFixturePackage("check");
		const auto generated = root / "src" / "generated";
		const auto manifest  = root / "quackconfig.yaml";
		assertEqual(0, checkPackage(root), "a new package is up to date");

		(void) writeFile(generated / "sl.dk", contents(generated / "sl.dk") + "# edited\n");
		assertEqual(1, checkPackage(root), "edited bindings");
		assertEqual(0, regenPackage(root, false, "duck"), "`regen` restores them");
		assertEqual(0, checkPackage(root), "up to date again");

		(void) writeFile(generated / "extra.dk", "");
		assertEqual(1, checkPackage(root), "a file the recipe does not generate");
		fs::remove(generated / "extra.dk");
		fs::remove(generated / "generated.dk");
		assertEqual(1, checkPackage(root), "a missing file");
		assertEqual(0, regenPackage(root, false, "duck"), "`regen` writes it again");

		auto       text = contents(manifest);
		const auto keys = std::string{ "  external-libs: [m]\n" };
		text.replace(
			text.find(keys), keys.size(), "  external-libs: [z]\n  external-lib-paths: [libs]\n"
		);
		(void) writeFile(manifest, text);
		assertEqual(1, checkPackage(root), "other library keys");
	}

	void bindingsWritesOnlyTheModules() {
		auto       root      = newFixturePackage("bindings-dir");
		const auto generated = root / "src" / "generated";
		const auto out       = work / "bindings_out";
		assertEqual(0, writeBindings(root / RECIPE_FILE, out, "sl"), "`bindings` succeeds");
		assertEqual(contents(generated / "sl.dk"), contents(out / "sl.dk"), "the bindings");
		assertEqual(
			contents(generated / "generated.dk"), contents(out / "generated.dk"), "the root module"
		);
		assertEqual(
			2,
			static_cast<int>(std::distance(fs::directory_iterator(out), fs::directory_iterator{})),
			"nothing else"
		);
		assertEqual(
			1, writeBindings(root / RECIPE_FILE, out, "my-lib"), "a name that is not an identifier"
		);
		assertEqual(
			1,
			writeBindings(root / RECIPE_FILE, out, std::nullopt),
			"the directory's name is not an identifier"
		);
		assertEqual(1, writeBindings(work / "none.yaml", out, "sl"), "no recipe");
		(void) writeFile(work / "bad" / RECIPE_FILE, "headers: [a.h]\nunknown: [x]\n");
		assertEqual(1, writeBindings(work / "bad" / RECIPE_FILE, out, "sl"), "invalid recipe");
	}

	void badInputIsReported() {
		auto root = newFixturePackage("bad_input");

		NewOptions again;
		again.at             = root;
		again.recipe.headers = { "sdl_like.h" };
		again.verify         = false;
		assertEqual(1, newPackage(again), "the package already exists");

		NewOptions broken;
		broken.at             = work / "broken";
		broken.recipe.headers = { path("broken.h") };
		broken.verify         = false;
		assertEqual(1, newPackage(broken), "the headers do not compile");
		assertTrue(!fs::exists(broken.at), "nothing is left behind");

		NewOptions bad_name = broken;
		bad_name.name       = "my-lib";
		assertEqual(1, newPackage(bad_name), "a name that is not an identifier");
		bad_name.name = "generated";
		assertEqual(1, newPackage(bad_name), "the name of the generated module");

		NewOptions no_headers;
		no_headers.at     = work / "no_headers";
		no_headers.verify = false;
		assertEqual(1, newPackage(no_headers), "no headers");

		fs::create_directories(work / "plain");
		assertEqual(1, regenPackage(work / "plain", false, "duck"), "`regen` without a recipe");
		assertEqual(1, checkPackage(work / "plain"), "`check` without a recipe");
		(void) writeFile(work / "plain" / RECIPE_FILE, "headers: []\n");
		assertEqual(1, regenPackage(work / "plain", false, "duck"), "an invalid recipe");
		(void) writeFile(work / "plain" / RECIPE_FILE, "headers: [sdl_like.h]\n");
		assertEqual(1, checkPackage(work / "plain"), "no manifest");
	}

	void verificationRunsDuck() {
		auto       root = newFixturePackage("verify");
		const auto bin  = work / "verify_bin";
		const auto regen
			= [&](const fs::path& duck) { return regenPackage(root, true, duck.string()); };
		fs::create_directories(bin);
		assertEqual(0, regen(fakeDuck(bin, "exit 0")), "the check passes");
		assertEqual(1, regen(fakeDuck(bin, "echo 1; exit 1")), "a layout differs");
		assertEqual(1, regen(fakeDuck(bin, "echo cannot link; exit 1")), "the package fails");
		assertEqual(1, regen(work / "no_such_duck"), "duck is missing");
		assertTrue(fs::exists(root / "quackconfig.yaml"), "the package is kept for inspection");
	}

	/// A package of `header_text`, named `ex`, in `work / dir_name`.
	fs::path newExtraPackage(const std::string& dir_name, const std::string& header_text) {
		auto root = work / dir_name;
		(void) writeFile(root / "ex.h", header_text);
		NewOptions options;
		options.at             = root;
		options.name           = "ex";
		options.recipe.headers = { (root / "ex.h").string() };
		options.verify         = false;
		assertEqual(0, newPackage(options), "`new` succeeds");
		return root;
	}

	void unsupportedDeclarationsAreSkippedWithReasons() {
		auto root     = newExtraPackage("unsupported", R"(#include <stdbool.h>
typedef enum ex_mode { EX_A, EX_B = 4 } ex_mode;
struct ex_flex { int n; int items[]; };
struct ex_bits { bool on: 1; unsigned big: 3; };
struct ex_outer { struct { int x; } inner; union { int i; float f; }; };
long double ex_long(long double x);
void ex_set_mode(ex_mode mode);
int ex_sum(int m[2][3]);
void ex_complex(float _Complex z);
void ex_take_flex(struct ex_flex* f);
void ex_take_bits(struct ex_bits* b);
void ex_take_outer(struct ex_outer* o);
int ex_global;
static const int EX_STATIC = 7;
static const float EX_RATIO = 0.5f;
)");
		auto bindings = contents(root / "src" / "generated" / "ex.dk");
		for (const auto* expected: { "fundecl ex_set_mode(mode: u32)",
		                             "fundecl ex_sum(m: cptr i32)",
		                             "const EX_STATIC: i32 = 7i32",
		                             "EX_B",
		                             "class ex_outer_inner",
		                             "ex_bits_set_on",
		                             "ex_flex_as_items",
		                             "`long double` has no mapping",
		                             "ex_global",
		                             "global variables are not supported",
		                             "ex_complex" })
			assertTrue(
				bindings.find(expected) != std::string::npos, std::format("has `{}`", expected)
			);
	}

	void pkgConfigPackagesAreResolved() {
		if (auto found = runProcess("pkg-config", { "--version" }); !found || found->exit_code != 0)
			return;
		const auto pc_dir = work / "pkgconfig";
		(void) writeFile(
			pc_dir / "ex.pc",
			std::format(
				"Name: ex\nDescription: ex\nVersion: 1.2.3\nCflags: -I{0}/include\n"
				"Libs: -L{0}/lib -lex\n",
				(work / "pc").string()
			)
		);
		(void) writeFile(work / "pc" / "include" / "ex.h", "int ex_answer(void);\n");
		os_utils::threadSafeSetenv("PKG_CONFIG_PATH", pc_dir.c_str(), 1);

		NewOptions options;
		options.at                = work / "pc";
		options.name              = "ex";
		options.recipe.headers    = { "ex.h" };
		options.recipe.pkg_config = { "ex" };
		options.verify            = false;
		assertEqual(0, newPackage(options), "`new` succeeds");
		const auto manifest = contents(options.at / "quackconfig.yaml");
		assertTrue(manifest.find("version: '1.2.3'") != std::string::npos, "the version");
		assertTrue(manifest.find("external-libs: [ex]") != std::string::npos, "the library");
		assertTrue(manifest.find("external-lib-paths: [lib]") != std::string::npos, "its directory");
		assertTrue(
			contents(options.at / "src" / "generated" / "ex.dk").find("(ex 1.2.3)")
				!= std::string::npos,
			"the banner names the version"
		);

		NewOptions missing        = options;
		missing.at                = work / "pc_missing";
		missing.recipe.pkg_config = { "duckbind_no_such_package" };
		assertEqual(1, newPackage(missing), "an unknown package");
		assertTrue(!fs::exists(missing.at), "nothing is left behind");
	}

	void manifestsThatCannotBeUpdatedAreReported() {
		auto       root     = newFixturePackage("manifests");
		const auto manifest = root / "quackconfig.yaml";
		const auto original = contents(manifest);

		auto block = original;
		block.replace(block.find("  external-libs: [m]"), 20, "  external-libs:\n    - m");
		(void) writeFile(manifest, block);
		assertEqual(1, regenPackage(root, false, "duck"), "a key in another shape");
		assertEqual(block, contents(manifest), "the manifest is not touched");

		(void) writeFile(manifest, "metadata:\n  version: '1.0.0'\n");
		assertEqual(1, checkPackage(root), "a manifest without a name");
		assertEqual(1, regenPackage(root, false, "duck"), "`regen` needs the name too");
	}
};

// Shares the fixtures of `integration_tests/tools/duckbind`.
TESTER_COMMON_MAIN("/integration_tests/tools/duckbind/")
