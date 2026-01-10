/**
 * @file lsp_daemon.cpp
 * @brief This file defines LSP daemon, the c++ layer of the duckling language server.
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
#include "go_to_definition.hpp"
#include "semantic_tokens.hpp"
#include "utils.hpp"
#include "validation.hpp"

#include <frontend/module_tree/module_tree.hpp>
#include <frontend/module_tree/queries.hpp>
#include <frontend/pst_parser/pst.hpp>

#include <clah/clah.hpp>
#include <filesystem/file.hpp>
#include <filesystem/file_path.hpp>
#include <init/init.hpp>
#include <lexer/lexer.hpp>
#include <query_framework/utils/with_context_do.hpp>

/**
 * @brief Starts the LSP server on the specified port.
 *
 * @param port The port number to run the server on.
 */
void server(i32 port) {
	crow::SimpleApp                           app;
	lsp::ExportKeywords                       lsp;
	std::unordered_map<std::string, fs::File> files;
	auto virtual_root = fs::FileManager::createRandomVirtualDirectory();

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


	/** @brief Route to init a directory contents recursively in the virtual file system.
	 * * URL: /init_directory/[base64 path]
	 * @param base64_path The base64 encoded absolute path of the root directory of the workspace.
	 * @return crow::response The HTTP response indicating the result of the operation.
	 */
	CROW_ROUTE(app, "/init_directory/<string>")
	([&virtual_root](const std::string& base64_path) {
		try {
			const auto path = fs::FilePath(base64::decode_into<std::string>(base64_path));

			auto virtual_path = lsp::initFiles(path, virtual_root);
			lsp::initModules(virtual_path);
			lsp::initPSTs(virtual_path);

			return crow::response(200, "OK");
		} catch (const std::exception& e) {
			std::cerr << e.what();
			return crow::response(400, e.what());
		}
	});

	/**
	 * @brief Route to add or override a file in the virtual file system.
	 * * URL: /put_file/[base64 relative path]/[base64 file contents]
	 * @param base64_path The base64 encoded relative path of the file.
	 * @param base64_content The base64 encoded content of the file.
	 * @return crow::response The HTTP response indicating the result of the operation.
	 */
	CROW_ROUTE(app, "/put_file/<string>/<string>")
	([&virtual_root](const std::string& base64_path, const std::string& base64_content) {
		try {
			const auto path    = base64::decode_into<std::string>(base64_path);
			const auto content = base64::decode_into<std::string>(base64_content);

			lsp::putFile(virtual_root, path, content);

			return crow::response(200, "OK");
		} catch (const std::exception& e) { return crow::response(400, e.what()); }
	});

	CROW_ROUTE(app, "/put_file/<string>/")
	([&virtual_root](const std::string& base64_path) {
		try {
			const auto path = base64::decode_into<std::string>(base64_path);

			lsp::putFile(virtual_root, path, "");

			return crow::response(200, "OK");
		} catch (const std::exception& e) { return crow::response(400, e.what()); }
	});

	/**
	 * @brief Route to generate diagnostics for a file under the given path in the virtual file
	 * system.
	 * * URL: /get_errors/[base64 relative path]
	 * @param base64_path The base64 encoded relative path of the file.
	 * @return crow::response The HTTP response containing the diagnostics.
	 */
	CROW_ROUTE(app, "/get_errors/<string>")
	([&virtual_root](const std::string& base64_path) {
		try {
			const auto path = base64::decode_into<std::string>(base64_path);
			auto       file = fs::File(virtual_root.getFilePath().join(path));
			if (not file.exists()) return crow::response(404, "File not found");

			auto json_str = lsp::getDiagnosticJsonFromCompiler(file);
			CROW_LOG_INFO << "Diagnostics:\n" << json_str;
			crow::response res(200, json_str);
			res.set_header("Content-Type", "application/json");
			return res;
		} catch (const std::exception& e) {
			CROW_LOG_ERROR << e.what();
			return crow::response(400, e.what());
		}
	});

	/**
	 * @brief Route to generate semantic tokens for a file under the given path in the virtual file
	 * system.
	 * * URL: /get_semantic_tokens/[base64 relative path]
	 * @param base64_path The base64 encoded relative path of the file.
	 * @return crow::response The HTTP response containing the semantic tokens in JSON format.
	 */
	CROW_ROUTE(app, "/get_semantic_tokens/<string>")
	([&virtual_root](const std::string& base64_path) {
		try {
			const auto relative_path = base64::decode_into<std::string>(base64_path);
			const auto path          = virtual_root.getFilePath().join(relative_path);

			if (!path.exists()) return crow::response(404, "File not found");

			const auto file        = fs::File(path);
			const auto file_vector = compiler::frontend::SourceFile::getSourceFilesfromFile(file);

			return crow::response(200, lsp::getSemanticTokens(file_vector));
		} catch (const std::exception& e) { return crow::response(400, e.what()); }
	});

	/**
	 * @brief Route to get definition location for a symbol defined by a given file and offset.
	 * * URL: /get_semantic_tokens/[base64 relative path]/[offset]
	 * @param base64_path The base64 encoded relative path of the file.
	 * @param offset The offset of the element
	 * @return crow::response The HTTP response containing the definition range in JSON format.
	 */
	CROW_ROUTE(app, "/get_definitions/<string>/<uint>")
	([&virtual_root](const std::string& base64_path, const uint& offset) {
		try {
			const auto relative_path = base64::decode_into<std::string>(base64_path);
			const auto path          = virtual_root.getFilePath().join(relative_path);

			if (!path.exists()) return crow::response(404, "File not found");

			const auto               file = fs::File(path);
			std::vector<std::string> out;

			query::utils::withContextDo([&file, &out, offset](query::Context& ctx) {
				auto src_files = compiler::frontend::SourceFile::getSourceFilesfromFile(file);
				for (auto& src_file: src_files) {
					auto pst = ctx.query<compiler::frontend::QueryFilePST>(src_file->getFileID());
					auto pst_root   = pst->getRootElement();
					auto element    = lsp::findElement(pst_root, offset, true);
					auto definition = lsp::findDefinition(element, ctx);
					if (definition.has_value())
						out.push_back("{" + definition.value().toJSON() + "}");
				}
			});

			return crow::response(200, lsp::jsonList(out));
		} catch (const std::exception& e) { return crow::response(400, e.what()); }
	});


	/**
	 * @brief Route to print interesting things from the daemon
	 * * URL: /debug
	 * @return whatever you want
	 */
	CROW_ROUTE(app, "/debug/<string>")
	([&virtual_root](const std::string& arg) {
		try {
			// put here whatever you want for debugging
			// use virtual_root to access the virtual file system
			std::cerr << "Debug arg: " << arg << "\n";
			std::cerr << "Virtual root path: " << virtual_root.getFilePath().string() << "\n";

			return crow::response(200, "OK");
		} catch (const std::exception& e) { return crow::response(400, e.what()); }
	});

	app.port(base::safeIntConv<u16>(port)).run();
}

/**
 * @brief Displays the version of the DucklingLS daemon.
 */
void showVersion() {
	std::cout << std::boolalpha;
	std::cout << "DucklingLS daemon version 0.0.\n";
}

clah::Clah getLspDaemonCLI() {
	return clah::Clah("lsp_daemon", "The Duckling Language Server Protocol daemon.")
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
 * Example usage 1: ./lsp_daemon start -p 8080
 * Example usage 2: ./lsp_daemon start --port 8080
 *
 * @param argc The number of command-line arguments.
 * @param argv The array of command-line arguments.
 * @return int The exit code of the program.
 */
int main(int argc, const char** argv) {
	// Initialize the command-line argument parser with help flag and port parameter
	auto clah = getLspDaemonCLI();

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
