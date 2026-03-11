#pragma once

#include <base/collections/maps.hpp>
#include <base/types/bit256.hpp>

#include <functional>

namespace debug_info {

	struct PstHash {
		u64 a;
		u64 b;
		u64 c;
		u64 d;
	};

	struct PstHashPostion {
		PstHash                 postion_scope_begin;
		base::Optional<PstHash> postion_scope_end;
	};

	struct FilePosition {
		std::string file_path;
		u64         start_line;
		u64         start_column;
		u64         end_line;
		u64         end_column;
	};

	struct SourcePosition {
		std::variant<PstHashPostion, FilePosition> line_col_position;
	};

	struct InstructionMetadata {
		SourcePosition position;
	};

	struct FunctionMetadata {
		base::Optional<std::string>    function_name;
		base::Optional<SourcePosition> position;

		std::vector<std::pair<u64, InstructionMetadata>> instr_offsets_to_metadata;
	};

	struct TypeMetadata {
		std::string name;
	};

	enum class Target { DBC };

	enum class SourcePositionsType { PstHash, LineColumn };

	struct DebugInfo {
		Target      target;         // DBC, LIR
		std::string module_path;    // path to the module this debug info is for
		SourcePositionsType
			source_positions_type;  // whether the debug info uses stable positions or not

		base::Map<std::string, FunctionMetadata> functions;
		base::Map<std::string, TypeMetadata>     types;

		/**
		 * @brief Resolves every PstHashPostion in this DebugInfo in-place.
		 *
		 * Covers the position of every FunctionMetadata entry and every
		 * InstructionMetadata within each function. Positions that are already
		 * FilePosition entries are left unchanged. Sets source_positions_type
		 * to SourcePositionsType::LineColumn.
		 *
		 * @param resolver A callable that maps PstHashPostion → FilePosition.
		 */
		void resolvePositions(const std::function<FilePosition(const PstHashPostion&)>& resolver);
	};
}
