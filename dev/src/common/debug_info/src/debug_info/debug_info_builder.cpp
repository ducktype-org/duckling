#include "debug_info_builder.hpp"

namespace debug_info {

	// --------------------------------------------------------------------------
	// FunctionBuilder
	// --------------------------------------------------------------------------

	FunctionBuilder::FunctionBuilder(
		DebugInfoBuilder& parent, std::string mangled_name, FunctionMetadata metadata
	):
		  parent(parent),
		  mangled_name(std::move(mangled_name)),
		  metadata(std::move(metadata)) {}

	FunctionBuilder& FunctionBuilder::addInstruction(u64 offset, SourcePosition position) {
		metadata.instr_offsets_to_metadata.emplace_back(
			offset, InstructionMetadata{ .position = std::move(position) }
		);
		return *this;
	}

	FunctionBuilder& FunctionBuilder::addVariableInit(
		u64 offset, std::string variable_name, base::Optional<SourcePosition> position
	) {
		metadata.instr_offsets_to_variable_init.emplace_back(
			offset,
			VariableMetadata{ .name = std::move(variable_name), .position = std::move(position) }
		);
		return *this;
	}

	FunctionBuilder& FunctionBuilder::addParameter(
		u64 index, std::string parameter_name, base::Optional<SourcePosition> position
	) {
		metadata.parameter_indexes_to_metadata.emplace_back(
			index,
			VariableMetadata{ .name = std::move(parameter_name), .position = std::move(position) }
		);
		return *this;
	}

	DebugInfoBuilder& FunctionBuilder::end() {
		parent.finalizeFunction(std::move(mangled_name), std::move(metadata));
		return parent;
	}

	// --------------------------------------------------------------------------
	// DebugInfoBuilder
	// --------------------------------------------------------------------------

	DebugInfoBuilder::DebugInfoBuilder(Target target, SourcePositionsType source_positions_type) {
		info.target                = target;
		info.source_positions_type = source_positions_type;
	}

	DebugInfoBuilder& DebugInfoBuilder::addType(std::string mangled_name, TypeMetadata metadata) {
		info.types.insertOrAssign(std::move(mangled_name), std::move(metadata));
		return *this;
	}

	DebugInfoBuilder& DebugInfoBuilder::addType(std::string mangled_name, std::string display_name) {
		return addType(std::move(mangled_name), TypeMetadata{ .name = std::move(display_name) });
	}

	FunctionBuilder DebugInfoBuilder::beginFunction(
		std::string                    mangled_name,
		base::Optional<std::string>    function_name,
		base::Optional<SourcePosition> position
	) {
		auto fun_metadata = FunctionMetadata{ .function_name = std::move(function_name),
			                                  .position      = std::move(position),
			                                  .parameter_indexes_to_metadata  = {},
			                                  .instr_offsets_to_metadata      = {},
			                                  .instr_offsets_to_variable_init = {} };
		return { *this, std::move(mangled_name), std::move(fun_metadata) };
	}

	void DebugInfoBuilder::finalizeFunction(std::string mangled_name, FunctionMetadata metadata) {
		info.functions.insertOrAssign(std::move(mangled_name), std::move(metadata));
	}

	DebugInfo DebugInfoBuilder::build() { return info; }
}  // namespace debug_info
