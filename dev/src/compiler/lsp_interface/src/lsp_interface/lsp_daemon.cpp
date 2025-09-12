/**
 * @file lsp_daemon.cpp
 * @brief This file defines LSP daemon, the c++ layer of the duckling language server.
 */

#include <clah/clah.hpp>

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

#include <frontend/module_tree/module_tree.hpp>
#include <frontend/module_tree/queries.hpp>
#include <pst_parser/pst.hpp>

#include <base/anycast.hpp>
#include <base/int_conv.hpp>
#include <base/macros/diagnostics.hpp>
#include <base/variant.hpp>

#include <filesystem/file.hpp>
#include <filesystem/file_path.hpp>
#include <lexer/lexer.hpp>
#include <query_framework/query_entry_point.hpp>
#include <query_framework/utils/with_context_do.hpp>

#include <vm/cli.hpp>
#include <vm/server.hpp>
#include <init/init.hpp>

/**
 * @brief Wrapper for converting API error to HTTP response.
 *
 * @param apiError The API error to convert.
 * @return crow::response The HTTP response corresponding to the API error.
 */
crow::response convertError(const vm::api::ApiError& apiError) {
	if (std::holds_alternative<vm::api::WrongResponse>(apiError)) return { 500, "Wrong response" };
	return {
		400,
		std::visit(
			[]([[maybe_unused]] const auto& v) {
				return "JSON is broken\n";  // JS::serializeStruct(v);
			},
			apiError
		),
	};
}

// // Debug
// std::string getContentsOfDirs(const fs::FsTree& tree) {
// 	std::string contents;
// 	for (const auto& [name, file]: tree.getFiles()) {
// 		contents += (name + ":\n");
// 		contents += ((std::string) file.getContent().view().stringView() + "\n\n\n");
// 	}

// 	for (const auto& [name, dir]: tree.getDirs()) contents += getContentsOfDirs(*dir);
// 	return contents;
// }

/**
 * @brief Converts the result of an operation to an HTTP response.
 *
 * @tparam E The type of the error.
 * @param x The result of the operation.
 * @return crow::response The HTTP response corresponding to the result.
 */
template<class E>
crow::response toResponse(const std::expected<void, E>& x) {
	static auto convert = []() { return crow::response(200, "{}"); };

	if (x.has_value()) return convert();
	return convertError(x.error());
}

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

			if (!virtual_root.getFilePath().join(path).exists()) {
				virtual_root.createSubFile(content, path);
			} else {
				auto file = fs::File(virtual_root.getFilePath().join(path));
				file.writeToFile(content);
			}

			return crow::response(200, "OK");
		} catch (const std::exception& e) { return crow::response(400, e.what()); }
	});

	CROW_ROUTE(app, "/put_file/<string>/")
	([&virtual_root](const std::string& base64_path) {
		try {
			const auto path = base64::decode_into<std::string>(base64_path);

			if (!virtual_root.getFilePath().join(path).exists()) {
				virtual_root.createSubFile("", path);
			} else {
				auto file = fs::File(virtual_root.getFilePath().join(path));
				file.writeToFile("");
			}

			return crow::response(200, "OK");
		} catch (const std::exception& e) { return crow::response(400, e.what()); }
	});


	CROW_ROUTE(app, "/make_module_tree/<string>")
	([&virtual_root](const std::string& base64_path) {
		try {
			const auto path = base64::decode_into<std::string>(base64_path);

			auto root = query::entryPoint<compiler::frontend::QueryModuleTree>(
				fs::File(virtual_root.getFilePath().join(path))
			);

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

			if (!virtual_root.getFilePath().join(path).exists())
				return crow::response(404, "File not found");

			auto              file   = fs::File(virtual_root.getFilePath().join(path));
			auto              tokens = lexer::tokenizeFile(file);
			pst::PST<>        pst(std::move(tokens));
			std::stringstream ss;
			if (pst.getLogger()->bad()) pst.getLogger()->dumpLog(true, ss);
			return crow::response(200, ss.str());
		} catch (const std::exception& e) { return crow::response(400, e.what()); }
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
			const auto path = virtual_root.getFilePath().join(relative_path);

			if (!path.exists()) return crow::response(404, "File not found");

			auto module = query::entryPoint<compiler::frontend::QueryModuleTree>(fs::File(path.parentPath()));

			const auto file = fs::File(path);
			const auto src_file = compiler::frontend::SourceFile::create(file, module);
			auto pst = src_file->getPST();

			if (pst->getLogger()->bad()) {
				std::stringstream ss;
				pst->getLogger()->dumpLog(true, ss);
				return crow::response(200, ss.str());
			};

			return crow::response(200, lsp::getSemanticTokens(pst->getRootElement()));
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
			const auto path = base64::decode_into<std::string>(base64_path);

			if (!fs::FilePath(virtual_root.getFilePath().join(path)).exists())
				return crow::response(404, "File not found");

			auto file = fs::File(virtual_root.getFilePath().join(path));

			// TODO: fix PST definition to enable definition finding
			auto       tokens = lexer::tokenizeFile(file);
			pst::PST<> pst(std::move(tokens));

			if (pst.getLogger()->bad()) {
				std::stringstream ss;
				pst.getLogger()->dumpLog(true, ss);
				return crow::response(200, ss.str());
			}

			auto pst_root   = pst.getRootElement();
			auto element    = lsp::findElement(pst_root, offset);
			auto definition = lsp::findDefinition(element);

			if (!definition.has_value()) return crow::response(200, "[]");

			std::vector<std::string> out = { definition.value().toJSON() };
			return crow::response(200, lsp::jsonList(out));
		} catch (const std::exception& e) { return crow::response(400, e.what()); }
	});


	/**
	 * @brief Route to print interesting things from the daemon
	 * * URL: /debug
	 * @return whatever you want
	 */
	CROW_ROUTE(app, "/debug/<string>")
	([&virtual_root](const std::string& base64_path) {
		try {
			std::string path = "/home/krzysiek/rift/duckling/dev/src/compiler/core/helios/tests/test_modules/expr_scopes";

			std::cout << "Requested path: " << path << "\n";
			//std::cout << "Virtual root path: " << virtual_root.getFilePath().string() << "\n";

			const auto& physfile = fs::File(path);
			//const auto& file = virtual_root.getFilePath().join(path);
			//const auto& fileobj = fs::File(file);
			//std::cout << "fileobj contynts: " << fileobj.getContent().view().stdString() << "\n";
			//std::cout << "Computed file path: " << file.exists() << "\n";
			//auto module_tree = module_builder->create(file);
			auto module = query::entryPoint<compiler::frontend::QueryModuleTree>(physfile);
			
			//auto module = module_tree->	getModuleID();
			//auto main_file = module_tree->getMainSourceFile();
			//std::cout << "Module tree:\n" << module_tree->prettyPrint() << "\n";

			
			auto pstr = base::anyCast<CRef<pst::PST<>>>(
				query::utils::withContextCompute([&](query::Context& ctx) {					
					auto main_file = ctx.query<compiler::frontend::QueryMainSourceFile>({ module });
					auto pst       = ctx.query<compiler::frontend::QueryFilePST>({ main_file });
					//std::cout << "Main File: " << main_file->getFileID().queryUnstablePerfectHash() << "     " <<  main_file->getFile().getContent().view().stdString() << "\n";
					return pst;
				})
			);
			std::cout << "WE GOT PST\n";

			return crow::response(200, "OK");
		} catch (const std::exception& e) { 
			std::cout << e.what() << "\n";
			return crow::response(400, e.what()); 
		}
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
			{ "[ERROR] ", printer::Color::RED },
			{ "Exception was caught with message:\n", printer::Color::DEFAULT },
			{ e.what(), printer::Color::DEFAULT },
			{ "\nAborting\n", printer::Color::DEFAULT },
		});
		return 1;
	} catch (const std::exception& e) {
		printer::StreamPrinter::print({
			{ "[ERROR] ", printer::Color::RED },
			{ "Unexpected Exception was caught with message:\n", printer::Color::DEFAULT },
			{ e.what(), printer::Color::DEFAULT },
			{ "\nAborting\n", printer::Color::DEFAULT },
		});
		return 1;
	}
}
