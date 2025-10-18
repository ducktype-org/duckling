#pragma once

#include <base/except/exceptions.hpp>

#include <nlohmann/json.hpp>

template<>
struct nlohmann::adl_serializer<std::exception> final {
	// NOLINTBEGIN(readability-identifier-naming)
	static void to_json(json& j, const std::exception& e) {
		j["name"] = typeid(e).name();  // It is not perfect, but there is nothing better.
		j["what"] = e.what();
	}

	static void from_json(const json&, std::exception&) {
		CORE_PANIC("Parsing data from JSON into exception is not supported (yet).");
	}

	// NOLINTEND(readability-identifier-naming)
};
