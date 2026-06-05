/**
 * @file main.cpp
 * @brief This file defines LS daemon, the c++ layer of the duckling language server.
 */

#include <base/preproc/diagnostics.hpp>  // this is included here, to provide push/pop diagnostics macros

#include <base64.hpp>

#include <iostream>

PUSH_DIAGNOSTIC;  // Our code is included after crow because of errors if pst was included earlier.
#pragma GCC diagnostic ignored "-Wuninitialized"
#include <crow/app.h>
#include <crow/http_response.h>
POP_DIAGNOSTIC;

#include "export_keywords.hpp"
#include "files_managment.hpp"
#include "go_to_definition.hpp"
#include "semantic_tokens.hpp"
#include "utils.hpp"
#include "validation.hpp"

#include <frontend/module_tree/module_flags/module_flags.hpp>
#include <frontend/module_tree/module_tree.hpp>
#include <frontend/module_tree/queries.hpp>
#include <frontend/pst_parser/pst.hpp>

#include <clah/clah.hpp>
#include <filesystem/file.hpp>
#include <filesystem/file_path.hpp>
#include <init/init.hpp>
#include <printer/stream_printer.hpp>
#include <query_framework/entry/with_context_do.hpp>
#include <query_framework/module_flags/module_flags.hpp>

/**
 * @brief Starts the LSP server on the specified port.
 *
 * @param port The port number to run the server on.
 */
void server(i32 port) {
	crow::SimpleApp     app;
	lsp::ExportKeywords lsp;

	/**
	 * @brief Route to check if the server is running.
	 * * URL: /status
	 * @return crow::response The HTTP response indicating the server status.
	 */
	CROW_ROUTE(app, "/status")
	([]() { return crow::response(200, "OK"); });

	/**
	 * @brief Route to export keywords.
	 * * URL: /export_keywords
	 * @return crow::response The HTTP response containing the exported keywords in JSON format.
	 */
	CROW_ROUTE(app, "/export_keywords")
	([lsp]() { return crow::response(200, lsp.getAllJson()); });


	/** @brief Register a workspace root so openFile knows when to stop walking upward.
	 * URL: /add_workspace/[base64 absolute path]
	 */
	CROW_ROUTE(app, "/add_workspace/<string>")
	([](const std::string& base64_path) {
		auto path = fs::FilePath(base64::decode_into<std::string>(base64_path));
		lsp::addWorkspace(path);
		return crow::response(200, "OK");
	});

	/** @brief Lazily initialise the package that owns the opened file.
	 * URL: /open_file/[base64 absolute path]
	 */
	CROW_ROUTE(app, "/open_file/<string>")
	([](const std::string& base64_path) {
		auto path = fs::FilePath(base64::decode_into<std::string>(base64_path));
		lsp::openFile(path);
		return crow::response(200, "OK");
	});

	/**
	 * @brief Route to add or override a file in the virtual file system.
	 * * URL: /put_file/[base64 absolute path]/[base64 file contents]
	 * @param base64_path The base64 encoded absolute path of the file.
	 * @param base64_content The base64 encoded content of the file.
	 * @return crow::response The HTTP response indicating the result of the operation.
	 */
	CROW_ROUTE(app, "/change_content/<string>/<string>")
	([](const std::string& base64_path, const std::string& base64_content) {
		const auto path    = base64::decode_into<std::string>(base64_path);
		const auto content = base64::decode_into<std::string>(base64_content);

		// This is because currently the `change_content` request can arrive
		// before the `open_file` request, so we need to make sure that the file is opened before
		// we try to change its content.
		lsp::openFile(path);
		lsp::updateFileContent(path, content);

		return crow::response(200, "OK");
	});

	/**
	 * This path is the extension of the previous one, and it is used when the second argument is
	 * empty, and the CROW can't handle it on it's own.
	 */
	CROW_ROUTE(app, "/change_content/<string>/")
	([](const std::string& base64_path) {
		const auto path = base64::decode_into<std::string>(base64_path);

		lsp::openFile(path);
		lsp::updateFileContent(path, "");

		return crow::response(200, "OK");
	});

	/**
	 * @brief Route to add or override a file in the virtual file system.
	 * * URL: /put_file/[base64 absolute path]/[base64 file contents]
	 * @param base64_path The base64 encoded absolute path of the file.
	 * @param base64_content The base64 encoded content of the file.
	 * @return crow::response The HTTP response indicating the result of the operation.
	 */
	CROW_ROUTE(app, "/add_file/<string>")
	([](const std::string& base64_path) {
		const auto path = base64::decode_into<std::string>(base64_path);

		lsp::addFile(path);

		return crow::response(200, "OK");
	});

	/**
	 * @brief Route to add or override a file in the virtual file system.
	 * * URL: /put_file/[base64 absolute path]/[base64 file contents]
	 * @param base64_path The base64 encoded absolute path of the file.
	 * @param base64_content The base64 encoded content of the file.
	 * @return crow::response The HTTP response indicating the result of the operation.
	 */
	CROW_ROUTE(app, "/remove_file_or_dir/<string>")
	([](const std::string& base64_path) {
		const auto path = base64::decode_into<std::string>(base64_path);

		lsp::removeFileOrDirectory(path);

		return crow::response(200, "OK");
	});

	/**
	 * @brief Route to generate diagnostics for a file under the given path in the virtual file
	 * system.
	 * * URL: /get_errors/[base64 absolute path]
	 * @param base64_path The base64 encoded absolute path of the file.
	 * @return crow::response The HTTP response containing the diagnostics.
	 */
	CROW_ROUTE(app, "/get_errors/<string>")
	([](const std::string& base64_path) {
		const auto path
			= fs::FilePath(base64::decode_into<std::string>(base64_path)).toVirtualPath();
		if (not path.exists()) return crow::response(404, "File not found");

		const auto file     = fs::File(path);
		auto       json_str = lsp::getDiagnosticJsonFromCompiler(file);
		CROW_LOG_INFO << "Diagnostics:\n" << json_str;

		crow::response res(200, json_str);
		res.set_header("Content-Type", "application/json");
		return res;
	});

	/**
	 * @brief Route to generate semantic tokens for a file under the given path in the virtual file
	 * system.
	 * * URL: /get_semantic_tokens/[base64 absolute path]
	 * @param base64_path The base64 encoded absolute path of the file.
	 * @return crow::response The HTTP response containing the semantic tokens in JSON format.
	 */
	CROW_ROUTE(app, "/get_semantic_tokens/<string>")
	([](const std::string& base64_path) {
		const auto path
			= fs::FilePath(base64::decode_into<std::string>(base64_path)).toVirtualPath();

		if (!path.exists()) return crow::response(404, "File not found");

		const auto file        = fs::File(path);
		const auto file_vector = compiler::frontend::SourceFile::getSourceFilesFromFile(file);

		return crow::response(200, lsp::getSemanticTokens(file_vector));
	});

	/**
	 * @brief Route to get definition location for a symbol defined by a given file and offset.
	 * * URL: /get_semantic_tokens/[base64 absolute path]/[offset]
	 * @param base64_path The base64 encoded absolute path of the file.
	 * @param offset The offset of the element
	 * @return crow::response The HTTP response containing the definition range in JSON format.
	 */
	CROW_ROUTE(app, "/get_definitions/<string>/<uint>")
	([](const std::string& base64_path, const uint& offset) {
		const auto path
			= fs::FilePath(base64::decode_into<std::string>(base64_path)).toVirtualPath();

		if (!path.exists()) return crow::response(404, "File not found");

		const auto               file = fs::File(path);
		std::vector<std::string> out;

		query::utils::withContextDo([&file, &out, offset](query::Context& ctx) {
			auto src_files = compiler::frontend::SourceFile::getSourceFilesFromFile(file);
			for (auto& src_file: src_files) {
				auto pst        = src_file->getPST();
				auto pst_root   = pst->getRootElement();
				auto element    = lsp::findElement(pst_root, offset, true);
				auto definition = lsp::findDefinition(element, ctx);
				if (definition.has_value()) out.push_back("{" + definition.value().toJSON() + "}");
			}
		});

		return crow::response(200, lsp::jsonList(out));
	});


	/**
	 * @brief Route to print interesting things from the daemon
	 * * URL: /debug
	 * @return whatever you want
	 */
	CROW_ROUTE(app, "/debug/<string>")
	([](const std::string& arg) {
		// put here whatever you want for debugging
		std::cerr << "Debug arg: " << arg << "\n";

		return crow::response(200, "OK");
	});

	app.port(base::safeIntConv<u16>(port)).concurrency(1).run();
}

/**
 * @brief Displays the version of the DucklingLS daemon.
 */
void showVersion() {
	std::cout << std::boolalpha;
	std::cout << "DucklingLS daemon version 0.0.\n";
}

clah::Clah getLspDaemonCLI() {
	return clah::Clah("duck_ls", "The Duckling Language Server daemon.")
	    .add(clah::ParamBuilder::ofFlag()
	             .addShortName('v')
	             .addLongName("version")
	             .addShortDesc("Show version information and exit")
	             .build())
	    .setPreHandler([](const clah::ParsingResult& options) {
			if (options.isFlag("version")) {
				showVersion();
				throw clah::exceptions::SuccessExitException(options);
			}
		})
	    .addSubcommand(clah::Clah("start", "Starts the LSP server on a given port")
	                       .add(clah::ParamBuilder::ofValue(clah::IntParser::make("port"))
	                                .addShortName('p')
	                                .addLongName("port")
	                                .addShortDesc("The port for the server to listen on.")
	                                .required()
	                                .build())
	                       .setHandler([](const clah::ParsingResult& options) {
							   auto port = options.getValue<i64>("port").value();
							   server(i32(port));
							   return 0;
						   }));
}

/**
 * @brief The main function of the LSP daemon.
 *
 * This function initializes the command-line argument parser, handles exceptions,
 * and starts the LSP server on the specified port.
 *
 * Example usage 1: ./duck_ls start -p 8080
 * Example usage 2: ./duck_ls start --port 8080
 *
 * @param argc The number of command-line arguments.
 * @param argv The array of command-line arguments.
 * @return int The exit code of the program.
 */
int main(int argc, const char** argv) {
	// Initialize the command-line argument parser with help flag and port parameter
	auto clah = getLspDaemonCLI();

	query::setTrackReverseGraph(true);
	compiler::frontend::use_module_modifier_remove = true;

	init::InitObject _;

	try {
		// Parse the command-line arguments
		return clah.execute(base::safeIntConv<usize>(argc), argv);
	} catch (const base::Exception& e) {
		printer::StreamPrinter::print({
			{ "[ERROR] ", printer::Color::Red },
			{ "Exception was caught with message:\n", printer::Color::Default },
			{ e.what(), printer::Color::Default },
			{ "\nAborting\n", printer::Color::Default },
		});
		return 1;
	} catch (const std::exception& e) {
		printer::StreamPrinter::print({
			{ "[ERROR] ", printer::Color::Red },
			{ "Unexpected Exception was caught with message:\n", printer::Color::Default },
			{ e.what(), printer::Color::Default },
			{ "\nAborting\n", printer::Color::Default },
		});
		return 1;
	}
}
