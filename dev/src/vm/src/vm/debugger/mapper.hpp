#pragma once

#include <debug_info/debug_info_io.hpp>

#include <base/extend_cpp/variant_match.hpp>

#include <diagnostic/location.hpp>
#include <diagnostic/source_position.hpp>
#include <filesystem/file.hpp>
#include <string_id/string_id.hpp>
#include <token_source/source.hpp>

#include <algorithm>
#include <fstream>

namespace vm::debugger {
	struct Mapper final {
		std::vector<debug_info::DebugInfo>                               infos;
		base::Map<base::StrID, base::CRef<debug_info::FunctionMetadata>> functions;

		base::Map<fs::FilePath, base::Box<tokenizer::TokenSource>> token_sources;

		using InstrOrVarMetadata = std::variant<
			base::CRef<debug_info::InstructionMetadata>,
			base::CRef<debug_info::VariableMetadata>>;

		base::Map<base::StrID, base::Map<usize, InstrOrVarMetadata>> function_instr_offsets;

		std::expected<void, std::string> loadMapping(fs::File mapping_file);

		base::Optional<InstrOrVarMetadata> mapCodePositionToMetadata(
			base::StrID function_name, usize instruction_index
		);

		base::Optional<debug_info::SourcePosition> getsp(
			const vm::debugger::Mapper::InstrOrVarMetadata& meta
		);

		debug_info::FilePosition getfp(const debug_info::SourcePosition& sp);

		base::Optional<debug_info::FilePosition> getfp(
			const base::Optional<debug_info::SourcePosition>& sp
		);

		base::Optional<debug_info::FilePosition> getfp(
			const vm::debugger::Mapper::InstrOrVarMetadata& meta
		);

		base::Optional<dia::SourcePosition> mapCodePositionToSourcePosition(
			base::StrID function_name, usize instruction_index
		);

		base::Optional<std::pair<base::StrID, usize>> mapSourcePositionToCodePosition(
			fs::FilePath filepath, usize line
		);

		std::optional<fs::FilePath> mainFile();
	};
}
