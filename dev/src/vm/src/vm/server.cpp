#include "server.hpp"

#include <base/misc/int_conv.hpp>
#include <base/preproc/diagnostics.hpp>

#include <vm/api/api.hpp>

#include <json/json.hpp>


PUSH_DIAGNOSTIC
#pragma GCC diagnostic ignored "-Wuninitialized"
#include <crow/app.h>
#include <crow/http_response.h>
POP_DIAGNOSTIC

crow::response convertError(const vm::api::ApiError& api_error) {
	if (std::holds_alternative<vm::api::WrongResponse>(api_error)) return { 500, "Wrong response" };
	return {
		400,
		nlohmann::json(api_error),
	};
}

template<class T, class E>
crow::response toResponse(const std::expected<T, E>& x) {
	static auto convert = []([[maybe_unused]]
	                         const auto& v) { return crow::response(200, nlohmann::json(v)); };

	if (x.has_value()) return convert(x.value());
	return convertError(x.error());
}

template<class E>
crow::response toResponse(const std::expected<void, E>& x) {
	static auto convert = []() { return crow::response(200, "{}"); };

	if (x.has_value()) return convert();
	return convertError(x.error());
}

void server(i32 port) {
	crow::SimpleApp app;

	CROW_ROUTE(app, "/status/<uint>")
	([](vm::PID pid) { return toResponse(vm::api::getExecutionStatus(pid)); });

	CROW_ROUTE(app, "/process/spawn").methods(crow::HTTPMethod::PUT)([]() {
		return toResponse(vm::api::spawn());
	});
	CROW_ROUTE(app, "/process/kill/<uint>").methods(crow::HTTPMethod::DELETE)([](vm::PID pid) {
		return toResponse(vm::api::kill(pid));
	});

	CROW_ROUTE(app, "/process/load/<uint>")
		.methods(crow::HTTPMethod::POST)([](const crow::request& req, vm::PID pid) {
			std::string filepath = req.body;

			return toResponse(vm::api::loadFiles(pid, { fs::File(filepath) }));
		});
	CROW_ROUTE(app, "/process/run/<uint>").methods(crow::HTTPMethod::POST)([](vm::PID pid) {
		return toResponse(vm::api::run(pid));
	});
	CROW_ROUTE(app, "/process/join/<uint>").methods(crow::HTTPMethod::POST)([](vm::PID pid) {
		return toResponse(vm::api::join(pid));
	});
	CROW_ROUTE(app, "/process/stop/<uint>").methods(crow::HTTPMethod::POST)([](vm::PID pid) {
		return toResponse(vm::api::stop(pid));
	});

	CROW_ROUTE(app, "/process/input/<uint>")
		.methods(crow::HTTPMethod::POST)([](const crow::request& req, vm::PID pid) {
			std::string input = req.body;

			return toResponse(vm::api::input(pid, input));
		});
	CROW_ROUTE(app, "/process/output/<uint>")
	([](vm::PID pid) { return toResponse(vm::api::output(pid)); });

	CROW_ROUTE(app, "/debug/pause/<uint>").methods(crow::HTTPMethod::POST)([](vm::PID pid) {
		return toResponse(vm::api::pause(pid));
	});
	CROW_ROUTE(app, "/debug/resume/<uint>").methods(crow::HTTPMethod::POST)([](vm::PID pid) {
		return toResponse(vm::api::resume(pid));
	});
	CROW_ROUTE(app, "/debug/step/<uint>").methods(crow::HTTPMethod::POST)([](vm::PID pid) {
		return toResponse(vm::api::step(pid));
	});

	CROW_ROUTE(app, "/data/type/<uint>/<string>")
	([](vm::PID pid, const std::string& type_name) {
		return toResponse(vm::api::getType(pid, type_name)
		                      .transform([](const vm::api::response::Type& type_response) {
								  return type_response.type;
							  }));
	});
	app.port(base::safeIntConv<u16>(port)).run();
}
