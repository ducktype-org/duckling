#pragma once

#include "base/stringifyable_enum.hpp"
#include "typesystem/higher/abstract_type.hpp"
#include <json/json.hpp>
#include <base/box.hpp>
#include <helios/symbols/symbols.hpp>
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

	template<>
	struct adl_serializer<compiler::helios::SymID> {
		static void to_json(json& j, const compiler::helios::SymID symbol) {
			j = { { "name", std::vector<std::string>{ compiler::helios::name(symbol).str() } },
				  { "kind", compiler::helios::kind(symbol) } };

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
			j = json{};
			for (auto s: symbols) j[std::to_string(s.customPerfectHash())] = s;
			// TODO: Ask how symbols are represented in lsp, because I don't see any way to
			// deserialize them.
		}
	};

}
