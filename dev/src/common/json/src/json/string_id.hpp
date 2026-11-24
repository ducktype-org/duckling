// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <string_id/string_id.hpp>

#include <nlohmann/json.hpp>

#include <string>

/**
 * @brief Serializes a `base::StrID` as the string it stands for, and a bad one as `null`.
 *
 * @note Without this, `base::StrID` is serialized by its ranges-friendly API, which makes it an
 * array of characters. Include this header wherever a `base::StrID` ends up in JSON.
 */
// NOLINTBEGIN(readability-identifier-naming)
template<>
struct nlohmann::adl_serializer<base::StrID> final {
	static void to_json(json& j, const base::StrID& str_id) {
		if (str_id.isBad())
			j = nullptr;
		else
			j = str_id.strView();
	}

	static void from_json(const json& j, base::StrID& str_id) {
		if (j.is_null())
			str_id = base::StrID();
		else
			str_id = base::StrID(j.get<std::string>());
	}
};

// NOLINTEND(readability-identifier-naming)
