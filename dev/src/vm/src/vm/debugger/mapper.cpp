// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "mapper.hpp"

#include <base/extend_cpp/variant_match.hpp>

#include <diagnostic/location.hpp>
#include <string_id/string_id.hpp>

#include <algorithm>
#include <fstream>

namespace {
	base::Optional<debug_info::SourcePosition> getsp(
		const vm::debugger::mapper::InstrOrVarMetadata& meta
	) {
		variant_match(meta) {
			variant_case(base::CRef<debug_info::InstructionMetadata>, m) { return m->position; }
			variant_case(base::CRef<debug_info::VariableMetadata>, m) { return m->position; }
		}

		return std::nullopt;
	}

	base::Optional<debug_info::FilePosition> getfp(const debug_info::SourcePosition& sp) {
		variant_match(sp.line_col_position) {
			variant_case(debug_info::FilePosition, fp) { return fp; }
			variant_default { return std::nullopt; }
		}
		CORE_UNREACHABLE();
	}

	base::Optional<debug_info::FilePosition> getfp(
		const base::Optional<debug_info::SourcePosition>& sp
	) {
		if_opt_some(sp, sp) { return getfp(sp); }
		return std::nullopt;
	}

	base::Optional<debug_info::FilePosition> getfp(
		const vm::debugger::mapper::InstrOrVarMetadata& meta
	) {
		return getfp(getsp(meta));
	}
}

namespace vm::debugger {
	std::expected<void, std::string> Mapper::loadMapping(const fs::File& mapping_file) {
		std::fstream stream(mapping_file.getFilePath().getPath());
		if (!stream.is_open()) return std::unexpected("Failed to open provided mapping file.");

		auto maybe_debug_info = debug_info::loadFromStream(stream);
		if (!maybe_debug_info) return std::unexpected(maybe_debug_info.error());
		infos.push_back(std::move(maybe_debug_info).value());

		// ensure no function duplicates (since we don't support overloading for FatBytecode)
		for (const auto& [name, metadata]: infos.back().functions) {
			auto fsid = base::StrID(name);
			if (functions.contains(fsid)) {
				infos.pop_back();
				return std::unexpected("This mapping file contains already mapped functions.");
			}
		}

		for (const auto& [name, metadata]: infos.back().functions) {
			auto fsid     = base::StrID(name);
			auto [it, _]  = functions.put(fsid, &metadata);
			auto& offsets = it->second.instr_offsets;

			for (const auto& [offset, meta]: metadata.instr_offsets_to_metadata)
				offsets.put(offset, &meta);
			for (const auto& [offset, meta]: metadata.instr_offsets_to_variable_init)
				offsets.put(offset, &meta);
		}

		return {};
	}

	base::Optional<mapper::InstrOrVarMetadata> Mapper::mapCodePositionToMetadata(
		base::StrID function_name, usize instruction_index
	) const {
		auto maybe_func = functions.atMaybe(function_name);
		if (!maybe_func) return std::nullopt;
		const auto& fio = (**maybe_func).instr_offsets;

		auto it = fio.upper_bound(instruction_index);
		if (it == fio.begin()) return std::nullopt;
		--it;

		return it->second;
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
		if (!file_path.exists()) return std::nullopt;

		if (!token_sources.contains(file_path)) {
			auto file  = fs::File(file_path);
			auto token = tokenizer::makeTokenSource(file);
			if (!token->tokenize()) return std::nullopt;
			token_sources.put(file_path, std::move(token));
		}
		const auto& token_source = token_sources.at(file_path);

		try {
			return dia::SourcePosition(
				token_source->getLocation(),
				token_source->getLine(fp->start_line).first + fp->start_column - 1,
				token_source->getLine(fp->end_line).first + fp->end_column - 1
			);
		} catch (const std::out_of_range&) {  // getLine
			return std::nullopt;
		} catch (const base::LogicError&) {   // SourcePosition
			return std::nullopt;
		}
	}

	base::Optional<std::pair<base::StrID, usize>> Mapper::mapSourcePositionToCodePosition(
		const fs::FilePath& filepath, usize line
	) const {
		for (const auto& [function_name, function]: functions) {
			// quick check if this function is worth considering
			match_optional(getfp(function.metadata->position)) {
				opt_none continue;
				opt_some(pos) {
					// ensure this function is in provided file
					if (pos.file_path != filepath) continue;

					// ensure provided line is in this function
					if (line < pos.start_line || pos.end_line < line) continue;
				}
			}

			// scan for the first FatBC starting not later then in that line (if such exists)
			usize ret = 0;
			for (const auto& [offset, meta]: function.instr_offsets) {
				auto pos = getfp(meta);
				if (!pos) continue;

				if (pos->start_line <= line) ret = offset;
				if (line <= pos->start_line) break;
			}
			return std::make_pair(function_name, ret);
		}

		return std::nullopt;
	}

	base::Optional<fs::FilePath> Mapper::mainFile() const {
		auto main = functions.atMaybe(base::StrID("main"));
		if (!main) return std::nullopt;

		auto fp = getfp((**main).metadata->position);
		if (!fp) return std::nullopt;

		return fp->file_path;
	}

	bool Mapper::containsFile(const fs::FilePath& filepath) const {
		return std::ranges::any_of(functions, [&](const auto& p) {
			const auto& [_, function] = p;
			auto function_position    = getfp(function.metadata->position);
			return function_position && function_position->file_path == filepath;
		});
	}
}
