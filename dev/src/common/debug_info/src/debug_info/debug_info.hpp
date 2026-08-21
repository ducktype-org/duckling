#pragma once

#include <base/collections/maps.hpp>
#include <base/types/bit256.hpp>

#include <ser/base/all.hpp>
#include <ser/ser.hpp>
#include <ser/std/all.hpp>

#include <functional>
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
		 * @brief Writes which alternative this is, then the alternative itself.
		 * @note The one hand-written pair in this module: `ser` walks aggregates by itself,
		 * but a variant is a choice rather than a field, and nothing in the object says which
		 * way the reader should go until a tag says so.
		 */
		static ::ser::Errc serWrite(::ser::writer auto& ar, const SourcePosition& self) {
			const auto tag = static_cast<u8>(self.line_col_position.index());
			if (const auto c = ar(tag); c != ::ser::Errc::Ok) return c;
			return std::visit(
				[&ar](const auto& position) { return ar(position); }, self.line_col_position
			);
		}

		/**
		 * @brief Reads the tag, then builds from the alternative it names.
		 * @note A `serMake` rather than a `serRead` because the variant is assigned as a
		 * whole: filling in place would mean reading into whichever alternative happens to be
		 * there already.
		 */
		static SourcePosition serMake(::ser::reader auto& ar) {
			switch (::ser::readField<u8>(ar).asInt()) {
			case 0:
				return SourcePosition{ ::ser::readField<PstHashPostion>(ar) };
			case 1:
				return SourcePosition{ ::ser::readField<FilePosition>(ar) };
			default:
				::ser::throwError(::ser::Errc::InvalidValue, ar.position());
			}
		}
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
	};
}
