#pragma once

#include "status.hpp"
#include <services_data/type_metadata/type.hpp>
#include <memory_data/block.hpp>

template<>
struct nlohmann::adl_serializer<base::RawView> {
	static void to_json(json& j, const base::RawView& e) { j = e.stringView(); }

	static void from_json(const json&, const base::RawView&) {
		CORE_PANIC("Parsing data from JSON into base::RawView is not supported (yet).");
	}
};

namespace vm::api {
	namespace response {
		struct Empty {};

		struct Output {
			std::string output;
			NLOHMANN_DEFINE_TYPE_INTRUSIVE(Output, output);
		};

		struct Block {
			base::RawView data;
			NLOHMANN_DEFINE_TYPE_INTRUSIVE(Block, data);
		};
	}

	using Response
		= std::variant<ProcStatus, response::Output, response::Block, TypeCRef, response::Empty>;
}
