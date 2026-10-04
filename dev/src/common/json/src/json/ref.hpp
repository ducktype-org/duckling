#pragma once

#include "empty_struct.hpp"
#include "type_parse.hpp"

#include <base/except/exceptions.hpp>
#include <base/pointers/ref.hpp>

#include <nlohmann/json.hpp>

// NOLINTBEGIN(readability-identifier-naming)
template<class T>
struct nlohmann::adl_serializer<Ref<T>> {
	static void to_json(json& j, const Ref<T>& v) {
		using DT  = std::decay_t<T>;
		j["type"] = std::string(TypeParseTraits<DT>::NAME.data());
		j["data"] = *v;
	}

	static void from_json(const json&, const Ref<T>&) {
		CORE_PANIC("Parsing data from JSON into ref is not supported (yet).");
	}
};

// NOLINTEND(readability-identifier-naming)
