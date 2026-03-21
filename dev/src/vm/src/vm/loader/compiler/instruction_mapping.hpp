#pragma once

#include <base/collections/maps.hpp>
#include <base/types/ints.hpp>

#include <string_id/string_id.hpp>
#include <diagnostic/source_position.hpp>
#include <filesystem/file.hpp>
#include <vm/api/data/response.hpp>
#include <vm/core/process/interface_types.hpp>


namespace vm::loader {
	struct MicroBytecodePosition {
		usize function_id;
		usize instruction_number;

		auto operator<=>(MicroBytecodePosition const&) const = default;
	};

	enum class LoadMode : uint8_t {
		Normal,
		WithMapping,
	};
};

namespace vm::loader::compiler {
	/**
	 * @brief Stores an debug symbols between fat-bytecode to micro-bytecode
	 */
	struct FatMicroMapping {
		struct FunctionCtx {
			struct EntryMicroToFat {
				usize fat_pos;
				base::Optional<dia::SourcePosition> src_pos;
			};

			base::HashMap<base::StrID, usize> label_to_code_offset{};
			base::HashMap<base::StrID, usize> varname_to_stack_offset{};
			base::HashMap<usize, usize>       high_to_low{};
			base::HashMap<usize, EntryMicroToFat> low_to_high{};

			/// @todo: Consider adding mapping from offset to variable name
		};

		struct FileCtx {
			std::map<FileCoordinates, api::response::CodePosition> coord_to_fat;
			/// @todo: check what other things in debugger are file-related
		};

		base::HashMap<fs::File, FileCtx>                     files;
		base::HashMap<u64, FunctionCtx>                      functions_ctx;
		base::HashMap<base::StrID, u64>                      funcname_to_id;

		base::Optional<MicroBytecodePosition> translateToMicroPos(
			const base::StrID& func_name, u64 instr_position
		) const {
			auto function_id = funcname_to_id.atMaybeCopy(func_name);

			if (!function_id) {
				return std::nullopt;
			}

			auto id = *function_id;
			if (!functions_ctx.contains(id)) {
				return std::nullopt;
			}

			auto& mapping = functions_ctx[id].high_to_low;
			auto instr_no = mapping.atMaybeCopy(instr_position);
			if (!instr_no) {
				return std::nullopt;
			}

			return MicroBytecodePosition {
				.function_id = id,
				.instruction_number = *instr_no
			};
		}

		base::Optional<api::response::CodePosition> translateToFatPos(
			const fs::File& file, const FileCoordinates& coord
		) const {
			if (!files.contains(file)) {
				return std::nullopt;
			}

			// @todo: check if the coordinates are actually in the range
			return files[file].coord_to_fat.lower_bound(coord)->second;
		}
	};
}