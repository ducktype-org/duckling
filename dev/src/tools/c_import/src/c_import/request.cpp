#include "request.hpp"

#include "emit.hpp"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <format>
#include <fstream>

namespace c_import {

	namespace {

		using nlohmann::json;

		std::vector<std::string> stringsAt(const json& object, const char* key) {
			if (!object.contains(key)) return {};
			return object.at(key).get<std::vector<std::string>>();
		}

		void writeFile(const std::filesystem::path& path, const std::string& content) {
			std::ofstream out(path, std::ios::binary | std::ios::trunc);
			out << content;
		}

	}

	std::expected<Request, std::string> parseRequest(std::string_view text) {
		try {
			auto    object = json::parse(text);
			Request request;
			request.module_name       = object.at("module_name").get<std::string>();
			request.bindings_import   = object.at("bindings_import").get<std::string>();
			request.output_dir        = object.at("output_dir").get<std::string>();
			request.read.headers      = stringsAt(object, "headers");
			request.read.clang_args   = stringsAt(object, "clang_args");
			request.read.resource_dir = object.value("resource_dir", std::string{});
			request.lower.include     = stringsAt(object, "include");
			request.lower.exclude     = stringsAt(object, "exclude");
			request.banner            = stringsAt(object, "banner");
			if (request.read.headers.empty()) return std::unexpected("no headers given");
			return request;
		} catch (const json::exception& error) {
			return std::unexpected(std::format("invalid request: {}", error.what()));
		}
	}

	std::string serializeReport(const Report& report) {
		json skipped = json::array();
		for (const auto& entry: report.module.skipped)
			skipped.push_back({ { "name", entry.name }, { "reason", entry.reason } });
		json layouts = json::array();
		for (const auto& layout: report.module.layouts) layouts.push_back(layout.name);

		json object{
			{ "errors", report.errors },
			{ "files", report.files },
			{ "layouts", layouts },
			{ "skipped", skipped },
			{ "counts",
			  {
				  { "classes", report.module.classes.size() },
				  { "functions", report.module.fundecls.size() },
				  { "constants", report.module.constants.size() },
				  { "generated_functions", report.module.functions.size() },
				  { "skipped", report.module.skipped.size() },
				  { "non_numeric_macros", report.non_numeric_macros },
			  } },
		};
		return object.dump(2) + "\n";
	}

	Report run(const Request& request) {
		Report report;
		auto   read               = readHeaders(request.read);
		report.errors             = std::move(read.errors);
		report.non_numeric_macros = read.non_numeric_macros;
		if (!report.errors.empty()) return report;

		report.module = lower(read.model, request.lower);

		std::filesystem::path directory{ request.output_dir };
		std::error_code       ec;
		std::filesystem::create_directories(directory, ec);
		if (ec) {
			report.errors.push_back(
				std::format("cannot create `{}`: {}", directory.string(), ec.message())
			);
			return report;
		}

		const auto bindings = request.module_name + ".dk";
		writeFile(directory / bindings, emitModule(report.module, request.banner));
		writeFile(
			directory / "layout_check.dk",
			emitLayoutCheck(report.module, request.banner, request.bindings_import)
		);
		report.files = { bindings, "layout_check.dk" };
		return report;
	}

}
