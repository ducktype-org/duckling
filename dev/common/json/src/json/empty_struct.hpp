#pragma once

#include <nlohmann/json.hpp>
#include "type_parse.hpp"
#include <base/exceptions.hpp>

template<class T>
requires std::is_empty_v<T> struct nlohmann::adl_serializer<T> {
	static void to_json(json& j, const T&) {
		j["type"] = std::string(TypeParseTraits<T>::name.data());
	}

	static void from_json(const json&, T&) {
		RIFT_PANIC("Parsing data from JSON into empty struct is not supported (yet).");
	}
};
