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

template<class T, class E>
crow::response toResponse(const cpp::result<T, E>& x) {
	static auto convert = []([[maybe_unused]] const auto& v) {
		return crow::response(200, /*JS::serializeStruct(v)*/ "{OK, json is broken}");
	};

	if (x.has_value()) return convert(x.value());
	return convertError(x.error());
}

template<class E>
crow::response toResponse(const cpp::result<void, E>& x) {
	static auto convert = []() { return crow::response(200, "{}"); };

	if (x.has_value()) return convert();
	return convertError(x.error());
}

void server(i32 port) {
	crow::SimpleApp app;
	pst::init();
	lsp::ExportKeywords                             lsp;
	std::unordered_map<std::string, fs::FilePath> files;

	/*
		/status

		Check if the server is running.
	*/
	CROW_ROUTE(app, "/status")
	([]() { return crow::response(200, "OK"); });

	/*
		/export_keywords

		Export keywords.
	*/
	CROW_ROUTE(app, "/export_keywords")
	([lsp]() { return crow::response(200, lsp.getAllJson()); });

	/*
		/put_file/[base64 relative path]/[base64 file contents]

		Add or override a file in the virtual file system.
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

	/*
		/get_lsptree/[base64 relative path]

		Generate LSP tree for a file under the given path in the virtual file system.
	*/
	CROW_ROUTE(app, "/get_lsptree/<string>")
	([&files](const std::string& base64_path) {
		try {
			const auto        path   = base64::decode_into<std::string>(base64_path);
			const auto        file   = files.at(path);
			auto              tokens = lexer::tokenizeFile(file);
			pst::PST<>        pst(std::move(tokens));
			std::stringstream ss;
			pst.getLSP(ss);
			return crow::response(200, ss.str());
		} catch (std::exception& e) {
			std::string error_msg = e.what();
			return crow::response(400, error_msg);
		}
	});

	/*
		/get_errors/[base64 relative path]

		Generate diagnostics for a file under the given path in the virtual file system.
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

void showVersion() {
	std::cout << std::boolalpha;
	std::cout << "DucklingLS daemon version 0.0.\n";
}

int main(int argc, const char** argv) {
	auto clap
		= clap::Clap().addHelpFlag().add(clap::ParamBuilder::ofValue(clap::IntParser::make("port"))
											 .addShortName('p')
											 .addLongName("port")
											 .addShortDesc("Choose port for server")
											 .build());

	clap::ParsingResult result;

	try {
		result = clap.parse(argc, argv);
	} catch (clap::exceptions::ClapException& e) {
		printer::StreamPrinter      console = printer::StreamPrinter();
		printer::PrinterContentsSeq contents;
		contents.emplace_back("rift: ", printer::Color::DEFAULT, printer::Color::DEFAULT);
		contents.emplace_back("error: ", printer::Color::RED, printer::Color::DEFAULT);
		contents.emplace_back(e.what(), printer::Color::DEFAULT, printer::Color::DEFAULT);
		console.printNL(contents);
		return 1;
	} catch (clap::exceptions::HelpException& e) {
		std::string help_message = clap::HelpMessageGenerator::generate(clap, e.parsing_result);
		std::cout << help_message << '\n';
		return 0;
	}

	if (result.isFlag('v'))
		showVersion();
	else if (auto port = result.getValue<i64>("port"))
		server(i32(port.value()));
	else
		throw std::runtime_error("No port or file specified");
}
