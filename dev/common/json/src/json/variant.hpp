#pragma once

#include <variant>
#include <nlohmann/json.hpp>
#include "empty_struct.hpp"
#include <type_traits>
#include "type_parse.hpp"

template<typename... Args>
struct nlohmann::adl_serializer<std::variant<Args...>> {
	static void to_json(json& j, const std::variant<Args...>& v) {
		std::visit(
			[&]<typename VT>(const VT& value) {
				using T   = std::decay_t<VT>;
				j["type"] = std::string(TypeParseTraits<T>::name.data());
				if constexpr (std::is_empty_v<T> || ::json::IsEmptySerialization<T>::value) {
				} else {
					j["data"] = value;
				}
			},
			v
		);
	}

	static void from_json(const json&, const std::variant<Args...>&) {}
};
