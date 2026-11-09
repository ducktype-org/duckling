// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <base/pointers/box.hpp>

#include <nlohmann/json.hpp>

// NOLINTBEGIN(readability-identifier-naming)
template<typename T>
struct nlohmann::adl_serializer<base::Box<T>> {
	static void to_json(json& j, const base::Box<T>& box) { j = *box; }

	static void from_json(const json&, base::Box<T>&) {
		CORE_PANIC("Parsing data from JSON into Box<T> not supported (yet).");
	}
};

// NOLINTEND(readability-identifier-naming)
