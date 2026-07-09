#pragma once

#include <debug_info/debug_info_io.hpp>

#include <diagnostic/source_position.hpp>
#include <filesystem/file.hpp>
#include <token_source/source.hpp>

namespace vm::debugger {
	namespace mapper {
		using InstrOrVarMetadata = std::variant<
			base::CRef<debug_info::InstructionMetadata>,
			base::CRef<debug_info::VariableMetadata>>;

		struct FunctionDI {
			base::CRef<debug_info::FunctionMetadata> metadata;
			base::Map<usize, InstrOrVarMetadata>     instr_offsets;
		};
	}

	class Mapper final {
		std::vector<debug_info::DebugInfo>         infos;
		base::Map<base::StrID, mapper::FunctionDI> functions;

		/**
		 * @brief collection of `tokenizer::TokenSource` for creating `dia::SourcePosition` from
		 * `debug_info::FilePosition`
		 */
		base::Map<fs::FilePath, base::Box<tokenizer::TokenSource>> token_sources;

		[[nodiscard]] base::Optional<mapper::InstrOrVarMetadata> mapCodePositionToMetadata(
			base::StrID function_name, usize instruction_index
		) const;

	public:
		/**
		 * @brief loads mapping from provided file to the `Mapper`
		 */
		std::expected<void, std::string> loadMapping(const fs::File& mapping_file);

		/**
		 * @brief maps function name and instruction index to source position
		 */
		base::Optional<dia::SourcePosition> mapCodePositionToSourcePosition(
			base::StrID function_name, usize instruction_index
		);

		/**
		 * @brief maps filepath and line to function name and instruction index
		 */
		[[nodiscard]] base::Optional<std::pair<base::StrID, usize>> mapSourcePositionToCodePosition(
			const fs::FilePath& filepath, usize line
		) const;

		/**
		 * @brief returns filepath of file containing function named `main`
		 */
		[[nodiscard]] base::Optional<fs::FilePath> mainFile() const;

		[[nodiscard]] bool containsFile(const fs::FilePath& filepath) const;
	};
}
