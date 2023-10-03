#include "server.hpp"

#include <crow.h>

#include <api/api.hpp>
#include <json/json.hpp>
#include <supervisor/supervisor.hpp>

crow::response convertError(const vm::api::ApiError& apiError) {
	if (std::holds_alternative<vm::api::WrongResponse>(apiError))
		return crow::response(500, "Wrong response");
	return crow::response(
		400,
		std::visit(
			[](const auto& v) {
				return "JSON is broken\n";  // JS::serializeStruct(v);
			},
			apiError
		)
	);
}

template<class T, class E>
crow::response toResponse(const result<T, E>& x) {
	static auto convert = [](const auto& v) {
		return crow::response(
			200,
			/*JS::serializeStruct(v)*/ "{OK, json is broken}"
		);
	};
	return convertResult<T, E, crow::response>(x, convert, convertError);
}

template<class E>
crow::response toResponse(const result<void, E>& x) {
	static auto convert = []() { return crow::response(200, "{}"); };
	return convertResult<E, crow::response>(x, convert, convertError);
}

void server(i32 port) {
	crow::SimpleApp app;

	CROW_ROUTE(app, "/status/<uint>")
	([](vm::PID pid) { return toResponse(vm::api::getExecutionStatus(pid)); });

	CROW_ROUTE(app, "/process/spawn").methods(crow::HTTPMethod::PUT)([]() {
		return toResponse(vm::api::spawn(false));
	});
	CROW_ROUTE(app, "/process/kill/<uint>").methods(crow::HTTPMethod::DELETE)([](vm::PID pid) {
		return toResponse(vm::api::kill(pid));
	});

	CROW_ROUTE(app, "/process/load/<uint>")
		.methods(crow::HTTPMethod::POST)([](const crow::request& req, vm::PID pid) {
			std::string filepath = req.body;

			return toResponse(vm::api::loadFile(pid, fs::FilePath(filepath)));
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
	([](vm::PID pid, std::string type_name) {
		return toResponse(vm::api::getType(pid, type_name).map([](const vm::TypeCRef& type_ptr) {
			return *type_ptr;
		}));
	});
	CROW_ROUTE(app, "/data/block/<uint>/<uint>")
	([](vm::PID pid, u32 block_id) {
		return toResponse(
			vm::api::getBlock(pid, block_id).map([](const vm::api::response::Block& block) {
				const byte* begin = block.data.getBegin();
				const byte* end   = begin + block.data.size();
				return std::vector<byte>(begin, end);
			})
		);
	});
	app.port(port).run();
}
