#pragma once

#include <exception>
#include <nlohmann/json.hpp>

template<>
struct nlohmann::adl_serializer<std::exception> {
	static void to_json(json& j, const std::exception& e) {
		j["name"] = typeid(e).name();  // It is not perfect, but there is nothing better.
		j["what"] = e.what();
	}

	static void from_json(const json&, std::exception&) {}
};
