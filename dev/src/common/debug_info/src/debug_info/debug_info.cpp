// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "debug_info.hpp"

#include <base/config/build_type.hpp>
#include <base/except/exceptions.hpp>

#include <variant>

namespace debug_info {

	namespace {

		SourcePosition resolvePosition(
			const SourcePosition&                                     pos,
			const std::function<FilePosition(const PstHashPostion&)>& resolver
		) {
			if (std::holds_alternative<FilePosition>(pos.line_col_position)) return pos;
			return SourcePosition{ .line_col_position
				                   = resolver(std::get<PstHashPostion>(pos.line_col_position)) };
		}

		/**
		 * @brief Calls `fun` on every source position stored in the debug info.
		 */
		template<typename Fun>
		void forEachPosition(
			base::Map<std::string, FunctionMetadata>& functions,
			base::Map<std::string, VariableMetadata>& globals,
			Fun                                       fun
		) {
			for (auto& [_, func]: functions) {
				if (func.position) fun(*func.position);

				for (auto& [_, parameter_meta]: func.parameter_indexes_to_metadata)
					if (parameter_meta.position) fun(*parameter_meta.position);

				for (auto& [_, instr_meta]: func.instr_offsets_to_metadata)
					fun(instr_meta.position);

				for (auto& [_, variable_meta]: func.instr_offsets_to_variable_init)
					if (variable_meta.position) fun(*variable_meta.position);
			}

			for (auto& [_, global_meta]: globals)
				if (global_meta.position) fun(*global_meta.position);
		}

	}  // namespace

	void DebugInfo::resolvePositions(
		const std::function<FilePosition(const PstHashPostion&)>& resolver
	) {
		source_positions_type = SourcePositionsType::LineColumn;

		forEachPosition(functions, globals, [&](SourcePosition& pos) {
			pos = resolvePosition(pos, resolver);
		});
	}

	void DebugInfo::mapPstPositions(const std::function<PstHashPostion(const PstHashPostion&)>& mapper
	) {
		forEachPosition(functions, globals, [&](SourcePosition& pos) {
			if (std::holds_alternative<PstHashPostion>(pos.line_col_position))
				pos.line_col_position = mapper(std::get<PstHashPostion>(pos.line_col_position));
		});
	}

	void DebugInfo::mergeFrom(DebugInfo&& other) {
		CORE_ASSERT(target == other.target, "Cannot merge debug info with different targets");
		CORE_ASSERT(
			source_positions_type == other.source_positions_type,
			"Cannot merge debug info with different source position types"
		);

		for (auto& [name, metadata]: other.functions) functions.emplace(name, std::move(metadata));
		for (auto& [name, metadata]: other.types) types.emplace(name, std::move(metadata));
		for (auto& [name, metadata]: other.globals) globals.emplace(name, std::move(metadata));
		auto _ = std::move(other);
	}

}  // namespace debug_info
