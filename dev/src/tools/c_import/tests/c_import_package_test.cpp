// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include <c_import/commands.hpp>
#include <c_import/generate.hpp>
#include <c_import/package.hpp>
#include <c_import/process.hpp>
#include <c_import/recipe.hpp>
#include <c_import/resolve.hpp>

#include <tester/tester.hpp>

#include <string>
#include <vector>

using namespace c_import;

namespace {

	using Strings = std::vector<std::string>;

	Resolved resolveLibraries(const Strings& libraries) {
		Recipe recipe;
		recipe.headers   = { "foo.h" };
		recipe.libraries = libraries;
		return resolve(recipe, "/work/pkg", false).value();
	}

	Resolved withLibraries(const Strings& libs, const Strings& paths) {
		Resolved resolved;
		resolved.external_libs      = libs;
		resolved.external_lib_paths = paths;
		return resolved;
	}

}

class CImportPackageTests final: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS CImportPackageTests

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(relativePaths);
		TESTER_ADD_TEST(recipeRoundTrips);
		TESTER_ADD_TEST(recipeRejectsUnknownKeys);
		TESTER_ADD_TEST(commandLinePathsBecomeRelativeToThePackage);
		TESTER_ADD_TEST(libraryNames);
		TESTER_ADD_TEST(flagsAndPathsBecomeNamesAndRelativeDirs);
		TESTER_ADD_TEST(standardAndPackageDirsAreNotMachineSpecific);
		TESTER_ADD_TEST(otherAbsoluteDirsAndFlagsAreReported);
		TESTER_ADD_TEST(objectFilesAreRejected);
		TESTER_ADD_TEST(newManifest);
		TESTER_ADD_TEST(libraryKeysAreReplacedKeepingEverythingElse);
		TESTER_ADD_TEST(libraryKeysAreAddedAndRemoved);
		TESTER_ADD_TEST(libraryKeysInAnotherShapeAreNotTouched);
		TESTER_ADD_TEST(packageNames);
		TESTER_ADD_TEST(plainVersions);
		TESTER_ADD_TEST(abiTriples);
	}

private:
	void relativePaths() {
		const std::filesystem::path root = "/work/pkg";
		assertEqual(
			std::string{ "include/a.h" }, relativePath("/work/pkg/include/a.h", root), "inside"
		);
		assertEqual(
			std::string{ "../vendor/a.h" }, relativePath("/work/vendor/a.h", root), "beside"
		);
		assertEqual(std::string{ "b.h" }, relativePath("/work/pkg/./x/../b.h", root), "normalized");
		assertEqual(std::string{ "." }, relativePath(root, root), "itself");
	}

	void recipeRoundTrips() {
		Recipe recipe;
		recipe.headers    = { "SDL3/SDL.h" };
		recipe.pkg_config = { "sdl3" };
		recipe.cflags     = { "-DFOO=1" };
		recipe.include    = { "SDL_*", "*Event" };
		auto text         = recipeText(recipe);
		assertTrue(text.starts_with("# Recipe of `duckbind`"), "header comment");
		auto parsed = parseRecipe(text);
		assertTrue(parsed.has_value(), "parses");
		assertTrue(*parsed == recipe, "round trip");
	}

	void recipeRejectsUnknownKeys() {
		assertTrue(!parseRecipe("headers: [a.h]\nlibs: [m]\n").has_value(), "unknown key");
		assertTrue(!parseRecipe("headers: a.h\n").has_value(), "not a list");
		assertTrue(!parseRecipe("pkg-config: [z]\n").has_value(), "no headers");
	}

	void commandLinePathsBecomeRelativeToThePackage() {
		TempDir     dir;
		const auto& cwd = dir.path();
		assertTrue(writeFile(cwd / "vendor" / "foo.h", "").has_value(), "header written");
		const auto outside = (cwd / "sdk" / "zlib.h").string();
		const auto inside  = (cwd / "pkg" / "include" / "bar.h").string();
		assertTrue(writeFile(outside, "").has_value(), "outside header written");
		assertTrue(writeFile(inside, "").has_value(), "inside header written");
		Recipe recipe;
		recipe.headers   = { "vendor/foo.h", "zlib.h", outside, inside };
		recipe.cflags    = { "-Ivendor", "-DFOO", "-I/opt/include", "-I" + (cwd / "pkg").string() };
		recipe.libraries = { "-Lbuild",
			                 "-lfoo",
			                 "build/libbar.a",
			                 "-L/usr/lib",
			                 "-L" + (cwd / "pkg" / "libs").string() };
		relativize(recipe, cwd, cwd / "pkg");
		assertTrue(
			recipe.headers == Strings{ "../vendor/foo.h", "zlib.h", outside, "include/bar.h" },
			"headers: absolute ones outside the package are kept"
		);
		assertTrue(
			recipe.cflags == Strings{ "-I../vendor", "-DFOO", "-I/opt/include", "-I." }, "cflags"
		);
		assertTrue(
			recipe.libraries
				== Strings{ "-L../build", "-lfoo", "../build/libbar.a", "-L/usr/lib", "-Llibs" },
			"libraries"
		);
	}

	void libraryNames() {
		assertEqual(std::string{ "foo" }, libraryName("libfoo.a"), ".a");
		assertEqual(std::string{ "foo" }, libraryName("libfoo.so"), ".so");
		assertEqual(std::string{ "foo" }, libraryName("libfoo.so.1.2"), ".so.N");
		assertEqual(std::string{ "foo" }, libraryName("libfoo.dylib"), ".dylib");
		assertEqual(std::string{}, libraryName("foo.o"), "object file");
		assertEqual(std::string{}, libraryName("lib.a"), "no name");
		assertEqual(std::string{}, libraryName("libfoo.sox"), "not .so");
	}

	void flagsAndPathsBecomeNamesAndRelativeDirs() {
		auto resolved = resolveLibraries({ "-Lbuild", "-lfoo", "vendor/libbar.a", "-lfoo" });
		assertTrue(resolved.external_libs == Strings{ "foo", "bar" }, "names");
		assertTrue(resolved.external_lib_paths == Strings{ "build", "vendor" }, "dirs");
		assertTrue(resolved.warnings.empty(), "no warnings");
	}

	void standardAndPackageDirsAreNotMachineSpecific() {
		auto resolved = resolveLibraries({ "-L/usr/lib", "-L/work/pkg/libs", "-lz" });
		assertTrue(resolved.external_lib_paths == Strings{ "libs" }, "dirs");
		assertTrue(resolved.warnings.empty(), "no warnings");
	}

	void otherAbsoluteDirsAndFlagsAreReported() {
		auto resolved = resolveLibraries({ "-L/opt/foo/lib", "-pthread", "-l:libz.so.1" });
		assertTrue(resolved.external_lib_paths == Strings{ "/opt/foo/lib" }, "kept");
		assertTrue(resolved.external_libs.empty(), "no names");
		assertEqual(std::size_t{ 3 }, resolved.warnings.size(), "warnings");
	}

	void objectFilesAreRejected() {
		Recipe recipe;
		recipe.headers   = { "foo.h" };
		recipe.libraries = { "build/foo.o" };
		assertTrue(!resolve(recipe, "/work/pkg", false).has_value(), "rejected");
	}

	void newManifest() {
		auto text = c_import::newManifest("sdl3", "3.4.16", withLibraries({ "SDL3" }, {}));
		assertEqual(
			std::string{
				"metadata:\n  name: sdl3\n  version: '3.4.16'\n  external-libs: [SDL3]\n" },
			text,
			"manifest"
		);
		assertEqual(std::string{ "sdl3" }, manifestName(text).value(), "name");
		assertTrue(manifestLibraries(text)->external_libs == Strings{ "SDL3" }, "libraries");
	}

	void libraryKeysAreReplacedKeepingEverythingElse() {
		const std::string manifest
			= "# mine\n"
			  "metadata:\n"
			  "  name: z\n"
			  "  version: '1.3.0'\n"
			  "  external-libs: [old]\n"
			  "  external-lib-paths: [gone]\n"
			  "  description: zlib bindings # keep\n"
			  "dependencies:\n"
			  "  other: '1'\n";
		auto updated = withLibraryKeys(manifest, withLibraries({ "z", "m" }, {}));
		assertEqual(
			std::string{ "# mine\n"
		                 "metadata:\n"
		                 "  name: z\n"
		                 "  version: '1.3.0'\n"
		                 "  external-libs: [z, m]\n"
		                 "  description: zlib bindings # keep\n"
		                 "dependencies:\n"
		                 "  other: '1'\n" },
			updated.value(),
			"updated"
		);
	}

	void libraryKeysAreAddedAndRemoved() {
		const std::string manifest = "metadata:\n    name: z\n\n# deps\ndependencies: {}\n";
		auto added = withLibraryKeys(manifest, withLibraries({ "z" }, { "libs" })).value();
		assertEqual(
			std::string{ "metadata:\n    name: z\n    external-libs: [z]\n"
		                 "    external-lib-paths: [libs]\n\n# deps\ndependencies: {}\n" },
			added,
			"added with the table's indentation, before what follows"
		);
		assertEqual(manifest, withLibraryKeys(added, withLibraries({}, {})).value(), "removed");
	}

	void libraryKeysInAnotherShapeAreNotTouched() {
		auto updated = withLibraryKeys(
			"metadata:\n  name: z\n  external-libs:\n    - z\n", withLibraries({ "z" }, {})
		);
		assertTrue(!updated.has_value(), "block list is reported");
		assertTrue(updated.error().contains("[z]"), "the values to set are named");
	}

	void packageNames() {
		assertTrue(packageNameError("sdl3").empty(), "identifier");
		assertTrue(!packageNameError("my-lib").empty(), "dash");
		assertTrue(!packageNameError("3d").empty(), "leading digit");
		assertTrue(!packageNameError("match").empty(), "keyword");
		assertTrue(!packageNameError("generated").empty(), "generated module");
	}

	void plainVersions() {
		assertTrue(isPlainVersion("3.4.16"), "x.y.z");
		assertTrue(!isPlainVersion("3.4"), "two parts");
		assertTrue(!isPlainVersion("1.2.3-beta"), "suffix");
		assertTrue(!isPlainVersion("1..3"), "empty part");
	}

	void abiTriples() {
		assertEqual(
			std::string{ "x86_64-unknown-linux-gnu" }, abiTriple("x86_64-pc-linux-gnu"), "vendor"
		);
		assertEqual(
			std::string{ "x86_64-unknown-linux-gnu" }, abiTriple("x86_64-linux-gnu"), "none"
		);
		assertEqual(
			std::string{ "x86_64-unknown-linux-musl" }, abiTriple("x86_64-alpine-linux-musl"), "musl"
		);
		assertEqual(
			std::string{ "arm64-apple-macosx" }, abiTriple("arm64-apple-macosx16.0.0"), "apple"
		);
		assertEqual(
			std::string{ "x86_64-unknown-linux-gnux32" },
			abiTriple("x86_64-pc-linux-gnux32"),
			"the environment keeps its digits"
		);
	}
};

TESTER_COMMON_MAIN("/src/tools/c_import/tests/")
