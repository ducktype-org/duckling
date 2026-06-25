#include "mapper.hpp"

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
	std::expected<void, std::string> Mapper::loadMapping(fs::File mapping_file) {
		std::fstream stream(mapping_file.getFilePath().getPath());
		auto         maybe_debug_info = debug_info::loadFromStream(stream);
		if (!maybe_debug_info) return std::unexpected(maybe_debug_info.error());
		auto di = maybe_debug_info.value();

		infos.push_back(di);
		for (const auto& [name, metadata]: infos.back().functions) {
			auto fsid = base::StrID(name);
			functions.put(fsid, &metadata);
			function_instr_offsets.put(fsid, {});
			for (const auto& [offset, meta]: metadata.instr_offsets_to_metadata)
				function_instr_offsets[fsid].put(offset, &meta);
			for (const auto& [offset, meta]: metadata.instr_offsets_to_variable_init)
				function_instr_offsets[fsid].put(offset, &meta);
		}

		return {};
	}

	base::Optional<Mapper::InstrOrVarMetadata> Mapper::mapCodePositionToMetadata(
		base::StrID function_name, usize instruction_index
	) {
		auto maybe_fio = function_instr_offsets.atMaybe(function_name);
		if (!maybe_fio) return std::nullopt;
		auto fio = maybe_fio.value();

		auto it = fio->upper_bound(instruction_index);
		if (it == fio->begin()) return std::nullopt;
		--it;

		return it->second;
	}

	base::Optional<debug_info::SourcePosition> Mapper::getsp(
		const vm::debugger::Mapper::InstrOrVarMetadata& meta
	) {
		variant_match(meta) {
			variant_case(base::CRef<debug_info::InstructionMetadata>, m) { return m->position; }
			variant_case(base::CRef<debug_info::VariableMetadata>, m) { return m->position; }
		}

		return std::nullopt;
	}

	debug_info::FilePosition Mapper::getfp(const debug_info::SourcePosition& sp) {
		return std::get<debug_info::FilePosition>(sp.line_col_position);
	}

	base::Optional<debug_info::FilePosition> Mapper::getfp(
		const base::Optional<debug_info::SourcePosition>& sp
	) {
		return sp.map([&](const debug_info::SourcePosition& spv) { return getfp(spv); });
	}

	base::Optional<debug_info::FilePosition> Mapper::getfp(
		const vm::debugger::Mapper::InstrOrVarMetadata& meta
	) {
		return getfp(getsp(meta));
	}

	base::Optional<dia::SourcePosition> Mapper::mapCodePositionToSourcePosition(
		base::StrID function_name, usize instruction_index
	) {
		auto maybe_meta = mapCodePositionToMetadata(function_name, instruction_index);
		if (!maybe_meta) return std::nullopt;
		auto meta = maybe_meta.value();

		auto fp = getfp(meta);
		if (!fp) return std::nullopt;

		auto file_path = fs::FilePath(fp->file_path);
		if (!token_sources.contains(file_path)) {
			auto file  = fs::File(file_path);
			auto token = tokenizer::makeTokenSource(file);
			token->tokenize();
			token_sources.put(file_path, std::move(token));
		}
		const auto& token_source = token_sources.at(file_path);

		return dia::SourcePosition(
			token_source->getLocation(),
			token_source->getLine(fp->start_line).first + fp->start_column - 1,
			token_source->getLine(fp->end_line).first + fp->end_column - 1
		);
	}

	base::Optional<std::pair<base::StrID, usize>> Mapper::mapSourcePositionToCodePosition(
		fs::FilePath filepath, usize line
	) {
		for (const auto& [function_name, offsets_map]: function_instr_offsets) {
			auto function_position = getfp(functions[function_name]->position);
			if (!function_position) continue;
			if (function_position->file_path != filepath || line < function_position->start_line
			    || function_position->end_line < line)
				continue;

			usize ret = 0;
			for (const auto& [offset, meta]: offsets_map) {
				auto pos = getfp(meta);
				if (!pos) continue;

				if (pos->start_line <= line) ret = offset;
				if (line <= pos->start_line) break;
			}
			return std::make_pair(function_name, ret);
		}

		return std::nullopt;
	}

	std::optional<fs::FilePath> Mapper::mainFile() {
		auto main = functions.atMaybe(base::StrID("main"));
		if (!main) return std::nullopt;

		auto fp = getfp((***main).position);
		if (!fp) return std::nullopt;

		return fp->file_path;
	}
}
