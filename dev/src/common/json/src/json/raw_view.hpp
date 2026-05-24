#pragma once

#include <base/except/exceptions.hpp>
#include <base/misc/raw_view.hpp>

#include <nlohmann/json.hpp>

// NOLINTBEGIN(readability-identifier-naming)
template<>
struct nlohmann::adl_serializer<base::RawView> {
	static void to_json(json& j, const base::RawView& e) { j = e.stringView(); }

	static void from_json(const json&, const base::RawView&) {
		CORE_PANIC("Parsing data from JSON into base::RawView is not supported (maybe yet).");
	}
};

// NOLINTEND(readability-identifier-naming)
