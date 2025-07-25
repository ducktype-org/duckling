#pragma once

#include "status.hpp"

#include <vm/core/process/memory/block.hpp>
#include <vm/core/process/type_metadata/type.hpp>

#include <memory>

// NOLINTBEGIN(readability-identifier-naming)
template<>
struct nlohmann::adl_serializer<base::RawView> {
	static void to_json(json& j, const base::RawView& e) { j = e.stringView(); }

	static void from_json(const json&, const base::RawView&) {
		CORE_PANIC("Parsing data from JSON into base::RawView is not supported (yet).");
	}
};

template<>
struct nlohmann::adl_serializer<vm::BlockID> {
	static void to_json(json& j, const vm::BlockID& e) { j = std::to_string(static_cast<u64>(e)); }

	static void from_json(const json& j, vm::BlockID& e) {
		e = static_cast<vm::BlockID>(std::stoull(j.get<std::string>()));
	}
};

// NOLINTEND(readability-identifier-naming)

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

		struct VmValue {
			std::shared_ptr<::vm::VmValue> vm_value;
			// TODOP: Fix that.
			NLOHMANN_DEFINE_TYPE_INTRUSIVE(VmValue, vm_value);
		};

		struct BlockIDs {
			std::vector<BlockID> ids;
			NLOHMANN_DEFINE_TYPE_INTRUSIVE(BlockIDs, ids);
		};

		struct CodePosition {
			u64 function_id;
			u64 instr_number;
			NLOHMANN_DEFINE_TYPE_INTRUSIVE(CodePosition, instr_number, function_id);
		};
	}

	using Response = std::variant<
		ProcStatus,
		response::Output,
		response::Block,
		TypeCRef,  // TODOP: Wouldn't t be nicer to return a TypeStruct?
		response::Empty,
		response::BlockIDs,
		response::CodePosition,
		response::VmValue,
		ExitValue>;
}
