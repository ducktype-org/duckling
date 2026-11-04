// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include "empty_struct.hpp"
#include "type_parse.hpp"

#include <base/except/exceptions.hpp>

#include <nlohmann/json.hpp>

#include <type_traits>
#include <variant>

template<typename... Args>
struct nlohmann::adl_serializer<std::variant<Args...>> {
	// NOLINTBEGIN(readability-identifier-naming)
	static void to_json(json& j, const std::variant<Args...>& v) {
		std::visit(
			[&]<typename VT>(const VT& value) {
				using T   = std::decay_t<VT>;
				j["type"] = std::string(js::typeName<T>());
				if constexpr (!std::is_empty_v<T>) j["data"] = value;
			},
			v
		);
	}

	static void from_json(const json&, const std::variant<Args...>&) {
		CORE_PANIC("Parsing data from JSON into a custom variant is not supported (yet).");
	}

	// NOLINTEND(readability-identifier-naming)
};
