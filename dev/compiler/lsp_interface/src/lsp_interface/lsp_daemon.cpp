/**
 * @file lsp_daemon.cpp
 * @brief This file defines LSP daemon, the c++ layer of the duckling language server.
 */
#include <iostream>
#include <iomanip>
#include <crow.h>
#include <unordered_map>
#include <base64.hpp>

#include <clap/clap.hpp>
#include <pst_parser/parser.hpp>
#include <pst_parser/pst.hpp>
#include <lexer/lexer.hpp>
#include <filesystem/file.hpp>

#include "cli.hpp"
#include "server.hpp"
#include "config.hpp"

#include "export_keywords.hpp"

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
			[]([[maybe_unused]]
		       const auto& v) {
				return "JSON is broken\n";  // JS::serializeStruct(v);
			},
			apiError
		),
	};
}

/**
 * @brief Converts the result of an operation to an HTTP response.
 *
 * @tparam E The type of the error.
 * @param x The result of the operation.
 * @return crow::response The HTTP response corresponding to the result.
 */
template<class E>
crow::response toResponse(const cpp::result<void, E>& x) {
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
	crow::SimpleApp app;
	pst::init();
	lsp::ExportKeywords                           lsp;
	std::unordered_map<std::string, fs::FilePath> files;

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
	([&files](const std::string& base64_path, const std::string& base64_content) {
		try {
			const auto path    = base64::decode_into<std::string>(base64_path);
			const auto content = base64::decode_into<std::string>(base64_content);
			const auto file    = fs::FilePath::createTempFile(content);
			files.erase(path);
			files.emplace(path, file);
			return crow::response(200, "OK");
		} catch (std::exception& e) {
			std::string error_msg = e.what();
			return crow::response(400, error_msg);
		}
	});

	/**
	 * @brief Route to generate LSP tree for a file under the given path in the virtual file system.
	 * * URL: /get_lsptree/[base64 relative path]
	 * @param base64_path The base64 encoded relative path of the file.
	 * @return crow::response The HTTP response containing the LSP tree.
	 */
	CROW_ROUTE(app, "/get_lsptree/<string>")
	([&files](const std::string& base64_path) {
		try {
			const auto        path   = base64::decode_into<std::string>(base64_path);
			const auto        file   = files.at(path);
			auto              tokens = lexer::tokenizeFile(file);
			pst::PST<>        pst(std::move(tokens));
			std::stringstream ss;

			// @TODO: replace it with some other LSP generation
			// pst.getLSP(ss);

			return crow::response(200, ss.str());
		} catch (std::exception& e) {
			std::string error_msg = e.what();
			return crow::response(400, error_msg);
		}
	});

	/**
	 * @brief Route to generate diagnostics for a file under the given path in the virtual file
	 * system.
	 * * URL: /get_errors/[base64 relative path]
	 * @param base64_path The base64 encoded relative path of the file.
	 * @return crow::response The HTTP response containing the diagnostics.
	 */
	CROW_ROUTE(app, "/get_errors/<string>")
	([&files](const std::string& base64_path) {
		try {
			const auto        path   = base64::decode_into<std::string>(base64_path);
			const auto        file   = files.at(path);
			auto              tokens = lexer::tokenizeFile(file);
			pst::PST<>        pst(std::move(tokens));
			std::stringstream ss;
			if (pst.getLogger().bad()) pst.getLogger().dumpLog(true, ss);
			return crow::response(200, ss.str());
		} catch (std::exception& e) {
			std::string error_msg = e.what();
			return crow::response(400, error_msg);
		}
	});

	app.port(port).run();
}

/**
 * @brief Displays the version of the DucklingLS daemon.
 */
void showVersion() {
	std::cout << std::boolalpha;
	std::cout << "DucklingLS daemon version 0.0.\n";
}

/**
 * @brief The main function of the LSP daemon.
 *
 * This function initializes the command-line argument parser, handles exceptions,
 * and starts the LSP server on the specified port.
 *
 * Example usage 1: ./lsp_daemon -p 8080
 * Example usage 2: ./lsp_daemon --port 8080
 *
 * @param argc The number of command-line arguments.
 * @param argv The array of command-line arguments.
 * @return int The exit code of the program.
 */
int main(int argc, const char** argv) {
	// Initialize the command-line argument parser with help flag and port parameter
	auto clap = clap::Clap()
	                .addHelpFlag()
	                .add(clap::ParamBuilder::ofFlag()
	                         .addShortName('v')
	                         .addLongName("version")
	                         .addShortDesc("Show version information")
	                         .build())
	                .add(clap::ParamBuilder::ofValue(clap::IntParser::make("port"))
	                         .addShortName('p')
	                         .addLongName("port")
	                         .addShortDesc("Choose port for server")
	                         .build());

	clap::ParsingResult result;

	try {
		// Parse the command-line arguments
		result = clap.parse(argc, argv);
	} catch (clap::exceptions::ClapException& e) {
		// Handle general parsing exceptions and print error message
		printer::StreamPrinter      console = printer::StreamPrinter();
		printer::PrinterContentsSeq contents;
		contents.emplace_back("duckling: ", printer::Color::DEFAULT, printer::Color::DEFAULT);
		contents.emplace_back("error: ", printer::Color::RED, printer::Color::DEFAULT);
		contents.emplace_back(e.what(), printer::Color::DEFAULT, printer::Color::DEFAULT);
		console.printNL(contents);
		return 1;
	} catch (clap::exceptions::HelpException& e) {
		// Handle help exception and print help message
		std::string help_message = clap::HelpMessageGenerator::generate(clap, e.parsing_result);
		std::cout << help_message << '\n';
		return 0;
	}

	if (result.isFlag("version")) showVersion();
	// Check if the port parameter is provided and start the server on the specified port
	else if (auto port = result.getValue<i64>("port"))
		server(i32(port.value()));
	else
		throw std::runtime_error("No port or file specified");
}
