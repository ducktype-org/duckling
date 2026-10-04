// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "test_utils.hpp"

#include <driver/initialize.hpp>
#include <frontend/packages/standard_packages.hpp>
#include <global_state/packages.hpp>
#include <lsp_interface/compiler.hpp>
#include <lsp_interface/files_cache.hpp>
#include <lsp_interface/server_session.hpp>

#include <tester/tester.hpp>

#include <algorithm>
#include <string>

class LspCompilerTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS LspCompilerTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(initializeAddsStdPackagesTest);
		TESTER_ADD_TEST(loadedPackageDependsOnStdTest);
		TESTER_ADD_TEST(stdTypesDiagnosticsTest);
	}

protected:
	void beforeAll() override {
		compiler::driver::initializeGlobalLogger();
		assertTrue(compiler.initialize().isOk(), "The compiler must initialize");
	}

private:
	duck_ls_test::StringStream      stream{ "" };
	lsp::ServerEndpoint             endpoint{ stream };
	duck_ls_test::CollectingSession session{ base::Ref<lsp::ServerEndpoint>(&endpoint) };
	duck_ls::FilesCache             files{ duck_ls_test::VfsWorkspace::vfs() };
	duck_ls::Compiler               compiler{
        base::Ref<duck_ls::ServerSession>(&session),
        base::Ref<duck_ls::FilesCache>(&files),
        compiler::driver::CompilerModeOfOperationAndOptions::LanguageServerMode{
						  .debug_options     = {},
						  .execution_options = {},
						  .stdlib_options
            = { .std_lib_type = compiler::driver::options_types::StdLibOptions::DefaultStd{} },
        },
	};

	/**
	 * @brief Every standard library package is registered by the initialization.
	 */
	void initializeAddsStdPackagesTest() {
		for (const auto& std_id: compiler::frontend::packages::standardLibraryPackageIds())
			assertTrue(
				global_state::getPackageRefOpt(std_id).has_value(),
				"Missing standard library package " + std_id.str()
			);
	}

	/**
	 * @brief A loaded package depends on every standard library package.
	 */
	void loadedPackageDependsOnStdTest() {
		duck_ls_test::VfsWorkspace workspace("lsp_compiler_ws");
		workspace.add("pkg/pkg.dk", "fun main() -> i64 = { return 0; }\n");

		const auto package_root = workspace.pathOf("pkg");
		compiler.loadPackage(package_root);

		auto package = global_state::getPackageRefOpt(duck_ls::packageIdForRoot(package_root));
		assertTrue(package.has_value(), "The loaded package must be registered");

		auto dependencies = package.value()->getDependencies().illegalAccess();
		for (const auto& std_id: compiler::frontend::packages::standardLibraryPackageIds())
			assertTrue(
				std::ranges::any_of(
					dependencies,
					[&](const auto& dependency) {
						return dependency.illegalAccess().getAlias() == std_id;
					}
				),
				"Missing dependency on " + std_id.str()
			);
	}

	/**
	 * @brief Opens a package using standard library types, then edits it to call a method the
	 * standard library type does not have and back, asserting what is published each time.
	 */
	void stdTypesDiagnosticsTest() {
		constexpr auto VALID_CONTENT
			= "fun main() -> i64 = {\n"
			  "    var text: String = \"abc\".toString();\n"
			  "    var list: List:{i64};\n"
			  "    list.push(1);\n"
			  "    return list.at(0);\n"
			  "}\n";
		constexpr auto INVALID_CONTENT
			= "fun main() -> i64 = {\n"
			  "    var text: String = \"abc\".toString();\n"
			  "    var list: List:{i64};\n"
			  "    list.add(1);\n"
			  "    return list.at(0);\n"
			  "}\n";

		duck_ls_test::VfsWorkspace workspace("lsp_compiler_std_ws");
		workspace.add("pkg/pkg.dk", VALID_CONTENT);
		const auto main_uri = workspace.uriOf("pkg/pkg.dk");

		compiler.addWorkspace(workspace.uriOf());
		compiler.openDocument(main_uri, "duckling", 1, VALID_CONTENT);
		compiler.publishDiagnostics(main_uri);
		assertTrue(session.noErrors(main_uri), "Using String and List:{i64} must not report errors");

		assertTrue(
			compiler.updateDocument(main_uri, 2, { wholeDocument(INVALID_CONTENT) }).isOk(),
			"The change must be applied"
		);
		compiler.publishDiagnostics(main_uri);
		assertTrue(session.hasErrors(main_uri), "Calling List.add must report an error");

		assertTrue(
			compiler.updateDocument(main_uri, 3, { wholeDocument(VALID_CONTENT) }).isOk(),
			"The change must be applied"
		);
		compiler.publishDiagnostics(main_uri);
		assertTrue(session.noErrors(main_uri), "The fixed file must not report errors");
	}

	/**
	 * @brief A change replacing the whole buffer with `text`.
	 */
	lsp::TextDocumentContentChangeEvent wholeDocument(std::string text) {
		return lsp::TextDocumentContentChangeWholeDocument{ .text = std::move(text) };
	}
};

TESTER_COMMON_MAIN("/src/compiler/lsp_interface/tests/");
