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

	FunctionBuilder& FunctionBuilder::addInstruction(u64 offset, InstructionMetadata metadata) {
		this->metadata.instr_offsets_to_metadata.emplace_back(offset, std::move(metadata));
		return *this;
	}

	FunctionBuilder& FunctionBuilder::addInstruction(u64 offset, SourcePosition position) {
		return addInstruction(offset, InstructionMetadata{ .position = std::move(position) });
	}

	DebugInfoBuilder& FunctionBuilder::end() {
		parent.finalizeFunction(std::move(mangled_name), std::move(metadata));
		return parent;
	}

	// --------------------------------------------------------------------------
	// DebugInfoBuilder
	// --------------------------------------------------------------------------

	DebugInfoBuilder::DebugInfoBuilder(
		Target target, std::string module_path, SourcePositionsType source_positions_type
	) {
		info.target                = target;
		info.module_path           = std::move(module_path);
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
		std::string mangled_name, std::string function_name, SourcePosition position
	) {
		return beginFunction(
			std::move(mangled_name),
			FunctionMetadata{ .function_name
		                      = base::Optional<std::string>{ std::move(function_name) },
		                      .position = base::Optional<SourcePosition>{ std::move(position) },
		                      .instr_offsets_to_metadata = {} }
		);
	}

	FunctionBuilder DebugInfoBuilder::beginFunction(
		std::string mangled_name, FunctionMetadata metadata
	) {
		return { *this, std::move(mangled_name), std::move(metadata) };
	}

	void DebugInfoBuilder::finalizeFunction(std::string mangled_name, FunctionMetadata metadata) {
		info.functions.insertOrAssign(std::move(mangled_name), std::move(metadata));
	}

	DebugInfo DebugInfoBuilder::build() { return std::move(info); }

}  // namespace debug_info
