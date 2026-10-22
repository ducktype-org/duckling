// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include "type_parse.hpp"

#include <base/except/exceptions.hpp>

#include <nlohmann/json.hpp>

template<class T>
requires std::is_empty_v<T> struct nlohmann::adl_serializer<T> final {
	// NOLINTBEGIN(readability-identifier-naming)
	static void to_json(json& j, const T&) {
		j["type"] = std::string(TypeParseTraits<T>::NAME.data());
	}

	static void from_json(const json&, T&) {
		CORE_PANIC("Parsing data from JSON into empty struct is not supported (yet).");
	}

	// NOLINTEND(readability-identifier-naming)
};
