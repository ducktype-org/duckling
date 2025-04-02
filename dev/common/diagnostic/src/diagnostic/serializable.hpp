#pragma once

#include <json/json.hpp>
#include <base/box.hpp>
#include <helios/symbols/symbols.hpp>

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
	struct adl_serializer<std::set<compiler::helios::SymID>> {
		static void to_json(json& j, const std::set<compiler::helios::SymID>& symbols) {
			j = json{};  // TODO: Actually do something here.
						 // This is easy, just use
			             // compiler::helios::name(SymID)
			             // compiler::helios::kind(SymID)
			             // SymID::customPerfectHash()
			// TODO: Also ask how symbols are represented in lsp, because I don't see any way to
			// deserialize them.
		}
	};
}
