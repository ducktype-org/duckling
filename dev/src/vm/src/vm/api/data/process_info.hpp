#pragma once

#include <base/extend_cpp/strongly_typed_id.hpp>
#include <base/types/ints.hpp>

#include <json/json.hpp>

namespace vm {
	/**
	 * @brief Process' ID.
	 */
	STRONG_TYPEDEF_ID_DIRECT_CREATION(PID);

	namespace api {
		struct ProcessInfo final {
			PID pid;

			NLOHMANN_DEFINE_TYPE_INTRUSIVE(ProcessInfo, pid);
		};
	}
}

template<>
struct nlohmann::adl_serializer<vm::PID> final {
	// NOLINTBEGIN(readability-identifier-naming)
	static void to_json(nlohmann::json& j, const vm::PID& pid) { j = pid.asInt(); }

	static void from_json(const nlohmann::json& j, vm::PID& pid) {
		pid = vm::PID::fromU64(j.get<u64>());
	}

	// NOLINTEND(readability-identifier-naming)
};

ID_STD_HASH(vm::PID);

JSON_REGISTER_TYPE_WITH_NAME(vm::api::ProcessInfo, "ProcessInfo")
