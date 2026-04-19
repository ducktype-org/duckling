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

	}  // namespace

	void DebugInfo::resolvePositions(
		const std::function<FilePosition(const PstHashPostion&)>& resolver
	) {
		source_positions_type = SourcePositionsType::LineColumn;

		for (auto& [_, func]: functions) {
			if (func.position) func.position = resolvePosition(*func.position, resolver);

			for (auto& [_, parameter_meta]: func.parameter_indexes_to_metadata)
				if (parameter_meta.position)
					parameter_meta.position = resolvePosition(*parameter_meta.position, resolver);

			for (auto& [_, instr_meta]: func.instr_offsets_to_metadata)
				instr_meta.position = resolvePosition(instr_meta.position, resolver);

			for (auto& [_, variable_meta]: func.instr_offsets_to_variable_init)
				if (variable_meta.position)
					variable_meta.position = resolvePosition(*variable_meta.position, resolver);
		}
	}

	void DebugInfo::mergeFrom(DebugInfo&& other) {
		IF_BUILD_TYPE_DEV({
			for (const auto& [name, _] : other.functions) {
				CORE_ASSERT(
					!functions.contains(name),
					"Duplicate function during DVM debug info merge: ",
					name
				);
			}
			for (const auto& [name, _] : other.types) {
				CORE_ASSERT(
					!types.contains(name), "Duplicate type during DVM debug info merge: ", name
				);
			}
		});

		for (auto& [name, metadata] : other.functions)
			functions.emplace(std::move(name), std::move(metadata));
		for (auto& [name, metadata] : other.types)
			types.emplace(std::move(name), std::move(metadata));
	}

}  // namespace debug_info

