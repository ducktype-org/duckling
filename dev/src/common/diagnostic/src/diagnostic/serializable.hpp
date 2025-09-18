#pragma once

#include "typesystem/higher/abstract_type.hpp"

#include <helios/scope_symbol_id.hpp>
#include <helios/symbols/simple.hpp>
#include <json/json.hpp>

#include "base/optional.hpp"
#include "base/stringifyable_enum.hpp"
#include <base/box.hpp>

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

	template<>
	struct adl_serializer<compiler::helios::SymID> {
		static void to_json(json& j, const compiler::helios::SymID symbol) {
			j = { { "kind", compiler::helios::kind(symbol) } };

			// Handle kind specific serialisation.
			switch (compiler::helios::kind(symbol)) {
			default:
				break;
			};
		}
	};

	template<>
	struct adl_serializer<tsh::AbstractType> {
		static void to_json(json& j, const tsh::AbstractType type) {
			j = { { "name", std::vector<std::string>{ type.toString() } },
				  { "kind", base::enumToStr(type.getKind()).str() } };

			// Handle kind specific serialisation.
			switch (type.getKind()) {
			default:
				break;
			};
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
				json symbol_json = s;
				symbol_json["assoc_infos"] = json::array();
				j[std::to_string(s.queryUnstablePerfectHash())] = symbol_json;
			} 
		}
	};

}
