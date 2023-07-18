#pragma once

#include <cstdint>
#include <json/json.hpp>

namespace vm {
	using PID = std::uint32_t;

	namespace api {
		struct ProcessInfo {
			PID pid;
			
			JS_OBJ(pid);
		};
	}
}

REGISTER_PARSE_TYPE_ALIAS(vm::api::ProcessInfo, "ProcessInfo")
