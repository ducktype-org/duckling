#include "debug_info.hpp"

#include <variant>

namespace debug_info {

namespace {

	SourcePosition resolvePosition(
const SourcePosition&                                      pos,
		const std::function<FilePosition(const PstHashPostion&)>& resolver
	) {
		if (std::holds_alternative<FilePosition>(pos.line_col_position))
			return pos;
		return SourcePosition{
			.line_col_position = resolver(std::get<PstHashPostion>(pos.line_col_position))
		};
	}

} // namespace

void DebugInfo::resolvePositions(
	const std::function<FilePosition(const PstHashPostion&)>& resolver
) {
	source_positions_type = SourcePositionsType::LineColumn;

	for (auto& [mangled, func]: functions) {
		func.position = resolvePosition(func.position, resolver);

		for (auto& [offset, instr_meta]: func.instr_offsets_to_metadata)
			instr_meta.position = resolvePosition(instr_meta.position, resolver);
	}
}

} // namespace debug_info
