// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <base/collections/maps.hpp>
#include <base/types/bit256.hpp>

#include <functional>
#include <ostream>
#include <string>
#include <string_view>
#include <variant>

namespace debug_info {
	struct PstHashPostion final {
		base::Bit256                 postion_scope_begin;
		base::Optional<base::Bit256> postion_scope_end;
	};

	struct FilePosition final {
		std::string file_path;
		u64         start_line;
		u64         start_column;
		u64         end_line;
		u64         end_column;
	};

	struct SourcePosition final {
		std::variant<PstHashPostion, FilePosition> line_col_position;

		/**
		 * @brief Returns a one-line human-readable form, e.g. "foo.duck:1:0 - 3:1" for a
		 * FilePosition or "pst[<begin hex> .. <end hex>]" for a PstHashPostion.
		 */
		[[nodiscard]] std::string toString() const;
	};

	struct InstructionMetadata final {
		SourcePosition position;
	};

	struct VariableMetadata final {
		std::string                    name;
		base::Optional<SourcePosition> position;
	};

	struct FunctionMetadata final {
		base::Optional<std::string>    function_name;
		base::Optional<SourcePosition> position;

		std::vector<std::pair<u64, VariableMetadata>> parameter_indexes_to_metadata;

		std::vector<std::pair<u64, InstructionMetadata>> instr_offsets_to_metadata;
		std::vector<std::pair<u64, VariableMetadata>>    instr_offsets_to_variable_init;
	};

	struct TypeMetadata final {
		std::string name;
	};

	enum class Target : u32 { DBC };

	enum class SourcePositionsType : u32 { PstHash, LineColumn };

	struct DebugInfo final {
		Target      target{};         // for now only DBC (maybe in future other targets)
		std::string module_path{};    // path to the module this debug info is for
		SourcePositionsType
			source_positions_type{};  // whether the debug info uses stable positions or not

		base::Map<std::string, FunctionMetadata> functions{};
		base::Map<std::string, TypeMetadata>     types{};

		/**
		 * @brief Resolves every PstHashPostion in this DebugInfo in-place.
		 *
		 * Covers the position of every FunctionMetadata entry and every
		 * InstructionMetadata and VariableMetadata (when present) within each
		 * function. Positions that are already
		 * FilePosition entries are left unchanged. Sets source_positions_type
		 * to SourcePositionsType::LineColumn.
		 *
		 * @param resolver A callable that maps PstHashPostion → FilePosition.
		 */
		void resolvePositions(const std::function<FilePosition(const PstHashPostion&)>& resolver);

		/**
		 * @brief Merges another DebugInfo into this one by inserting all its functions and types.
		 * Does not perform any assertions.
		 */
		void mergeFrom(DebugInfo&& other);

		/**
		 * @brief Writes a human-readable dump of the whole DebugInfo to @p os.
		 */
		void debugPrint(std::ostream& os) const;

		/**
		 * @brief Returns what debugPrint() writes as a string.
		 */
		[[nodiscard]] std::string toString() const;
	};

}
