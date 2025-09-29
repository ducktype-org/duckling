#pragma once

#include <helios/scope_symbol_id.hpp>
#include <typesystem/higher/abstract_type.hpp>

#include "base/optional.hpp"
#include "base/stringifyable_enum.hpp"
#include <base/box.hpp>

#include <json/json.hpp>

#include <set>
#include <type_traits>

namespace dia {
	template<typename T>
	concept Serializable = requires(T t) {
		{ t.tojson() } -> std::same_as<nlohmann::json>;
	};
}

namespace nlohmann {
	template<dia::Serializable T>
	struct adl_serializer<Box<T>> {
		static void to_json(json& j, const Box<T>& opt) { j = opt->tojson(); }
	};

	template<typename T>
	struct adl_serializer<base::Optional<T>> {
		static void to_json(json& j, const base::Optional<T>& opt) {
			if_opt_some(opt, val) { j = val; }
			if_opt_none(opt) { j = json{}; }
		}
	};

	template<typename T>
	concept SupportedType
		= (std::same_as<T, compiler::helios::SymID> || std::same_as<T, tsh::AbstractType>);

	template<SupportedType T>
	struct adl_serializer<std::set<T>> {
		static void to_json(json& j, const std::set<T>& symbols) {
			j = json::object();
			for (auto& s: symbols) {
				json symbol_json                                = s;
				symbol_json["assoc_infos"]                      = json::array();
				j[std::to_string(s.queryUnstablePerfectHash())] = symbol_json;
			}
		}
	};

}
