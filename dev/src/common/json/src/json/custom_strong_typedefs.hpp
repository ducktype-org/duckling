#pragma once

#include <base/types/bits_and_bytes.hpp>

#include <nlohmann/json.hpp>

#define JSON_REGISTER_CUSTOM_STRONG_TYPEDEF(TYPE)                                            \
	template<>                                                                               \
	struct nlohmann::adl_serializer<TYPE> final {                                            \
		static void to_json(json& j, const TYPE& t) {                                        \
			j["name"]  = #TYPE;                                                              \
			j["value"] = t.asInt();                                                          \
		}                                                                                    \
                                                                                             \
		static void from_json(const json& j, TYPE& t) { t = TYPE(j["value"].get<usize>()); } \
	};

JSON_REGISTER_CUSTOM_STRONG_TYPEDEF(Bits);
JSON_REGISTER_CUSTOM_STRONG_TYPEDEF(Bytes);
