// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "debug_info_io.hpp"

#include <json/json.hpp>

#include <algorithm>
#include <expected>
#include <istream>
#include <ostream>

// NOLINTBEGIN

namespace nlohmann {
	template<typename K, typename V>
	struct adl_serializer<base::Map<K, V>> {
		static void to_json(json& j, const base::Map<K, V>& map) {
			j = json::object();
			for (const auto& [k, v]: map) j[k] = v;
		}

		static void from_json(const json& j, base::Map<K, V>& map) {
			map.clear();
			for (auto it = j.begin(); it != j.end(); ++it)
				map.insertOrAssign(it.key(), it.value().get<V>());
		}
	};

	template<>
	struct adl_serializer<base::Bit256> {
		static void to_json(json& j, const base::Bit256& v) { j = v.data; }

		static void from_json(const json& j, base::Bit256& v) {
			std::array<u64, 4> arr{};
			j.get_to(arr);
			v = base::Bit256(arr);
		}
	};
}  // namespace nlohmann

namespace debug_info {
	using json = nlohmann::json;

	// -- PstHashPostion

	void to_json(json& j, const PstHashPostion& v) {
		j = json{ { "position_scope_begin", v.postion_scope_begin } };
		if (v.postion_scope_end) j["position_scope_end"] = *v.postion_scope_end;
	}

	void from_json(const json& j, PstHashPostion& v) {
		j.at("position_scope_begin").get_to(v.postion_scope_begin);
		if (j.contains("position_scope_end"))
			v.postion_scope_end = j.at("position_scope_end").get<base::Bit256>();
	}

	// -- FilePosition

	void to_json(json& j, const FilePosition& v) {
		j = json{ { "file_path", v.file_path },
			      { "start_line", v.start_line },
			      { "start_column", v.start_column },
			      { "end_line", v.end_line },
			      { "end_column", v.end_column } };
	}

	void from_json(const json& j, FilePosition& v) {
		j.at("file_path").get_to(v.file_path);
		j.at("start_line").get_to(v.start_line);
		j.at("start_column").get_to(v.start_column);
		j.at("end_line").get_to(v.end_line);
		j.at("end_column").get_to(v.end_column);
	}

	// -- SourcePosition

	void to_json(json& j, const SourcePosition& v) {
		if (std::holds_alternative<PstHashPostion>(v.line_col_position)) {
			j = { { "type", "PstHashPosition" },
				  { "value", std::get<PstHashPostion>(v.line_col_position) } };
		} else {
			j = { { "type", "FilePosition" },
				  { "value", std::get<FilePosition>(v.line_col_position) } };
		}
	}

	void from_json(const json& j, SourcePosition& v) {
		const std::string type = j.at("type");
		if (type == "PstHashPosition")
			v.line_col_position = j.at("value").get<PstHashPostion>();
		else if (type == "FilePosition")
			v.line_col_position = j.at("value").get<FilePosition>();
	}

	// -- InstructionMetadata

	void to_json(json& j, const InstructionMetadata& v) { j = json{ { "position", v.position } }; }

	void from_json(const json& j, InstructionMetadata& v) { j.at("position").get_to(v.position); }

	// -- VariableMetadata

	void to_json(json& j, const VariableMetadata& v) {
		j         = json::object();
		j["name"] = v.name;
		if (v.position) j["position"] = *v.position;
	}

	void from_json(const json& j, VariableMetadata& v) {
		j.at("name").get_to(v.name);
		if (j.contains("position") && !j.at("position").is_null())
			v.position = j.at("position").get<SourcePosition>();
		else
			v.position = std::nullopt;
	}

	// -- TypeMetadata

	void to_json(json& j, const TypeMetadata& v) { j = json{ { "name", v.name } }; }

	void from_json(const json& j, TypeMetadata& v) { j.at("name").get_to(v.name); }

	// -- FunctionMetadata

	void to_json(json& j, const FunctionMetadata& v) {
		// Sort a temporary copy by offset so the on-disk format is always ordered.
		auto sorted_instr_offsets_to_metadata = v.instr_offsets_to_metadata;
		std::ranges::sort(
			sorted_instr_offsets_to_metadata,
			{},
			&decltype(sorted_instr_offsets_to_metadata)::value_type::first
		);
		auto sorted_instr_offsets_to_variable_init = v.instr_offsets_to_variable_init;
		std::ranges::sort(
			sorted_instr_offsets_to_variable_init,
			{},
			&decltype(sorted_instr_offsets_to_variable_init)::value_type::first
		);
		auto sorted_parameter_indexes_to_metadata = v.parameter_indexes_to_metadata;
		std::ranges::sort(
			sorted_parameter_indexes_to_metadata,
			{},
			&decltype(sorted_parameter_indexes_to_metadata)::value_type::first
		);

		// Build explicitly: Optional<string> and Optional<SourcePosition> aren't
		// in the debug_info namespace, so ADL won't find our generic to_json helpers
		// if we put them in a braced initializer list.
		j = json::object();
		if (v.function_name) j["function_name"] = *v.function_name;
		if (v.position) j["position"] = *v.position;
		j["parameter_indexes_to_metadata"]  = sorted_parameter_indexes_to_metadata;
		j["instr_offsets_to_metadata"]      = sorted_instr_offsets_to_metadata;
		j["instr_offsets_to_variable_init"] = sorted_instr_offsets_to_variable_init;
	}

	void from_json(const json& j, FunctionMetadata& v) {
		// Optional<string> — handle explicitly (ADL won't find our Optional helper).
		if (j.contains("function_name") && !j.at("function_name").is_null())
			v.function_name = j.at("function_name").get<std::string>();
		else
			v.function_name = std::nullopt;
		// Optional<SourcePosition> — same reason.
		if (j.contains("position") && !j.at("position").is_null())
			v.position = j.at("position").get<SourcePosition>();
		else
			v.position = std::nullopt;
		if (j.contains("parameter_indexes_to_metadata"))
			j.at("parameter_indexes_to_metadata").get_to(v.parameter_indexes_to_metadata);
		else
			v.parameter_indexes_to_metadata.clear();
		j.at("instr_offsets_to_metadata").get_to(v.instr_offsets_to_metadata);
		if (j.contains("instr_offsets_to_variable_init"))
			j.at("instr_offsets_to_variable_init").get_to(v.instr_offsets_to_variable_init);
		else
			v.instr_offsets_to_variable_init.clear();
		if (!std::ranges::is_sorted(
				v.parameter_indexes_to_metadata,
				{},
				&decltype(v.parameter_indexes_to_metadata)::value_type::first
			))
			throw nlohmann::json::other_error::create(
				501, "parameter_indexes_to_metadata is not sorted by index", &j
			);
		if (!std::ranges::is_sorted(
				v.instr_offsets_to_metadata,
				{},
				&decltype(v.instr_offsets_to_metadata)::value_type::first
			))
			throw nlohmann::json::other_error::create(
				501, "instr_offsets_to_metadata is not sorted by offset", &j
			);
		if (!std::ranges::is_sorted(
				v.instr_offsets_to_variable_init,
				{},
				&decltype(v.instr_offsets_to_variable_init)::value_type::first
			))
			throw nlohmann::json::other_error::create(
				501, "instr_offsets_to_variable_init is not sorted by offset", &j
			);
	}

	// -- Target / SourcePositionsType enums

	NLOHMANN_JSON_SERIALIZE_ENUM(Target, { { Target::DBC, "DBC" } })
	NLOHMANN_JSON_SERIALIZE_ENUM(
		SourcePositionsType,
		{ { SourcePositionsType::PstHash, "PstHash" },
	      { SourcePositionsType::LineColumn, "LineColumn" } }
	)

	// -- DebugInfo

	void to_json(json& j, const DebugInfo& v) {
		j = json{ { "target", v.target },
			      { "module_path", v.module_path },
			      { "source_positions_type", v.source_positions_type },
			      { "functions", v.functions },
			      { "types", v.types } };
	}

	void from_json(const json& j, DebugInfo& v) {
		j.at("target").get_to(v.target);
		j.at("module_path").get_to(v.module_path);
		j.at("source_positions_type").get_to(v.source_positions_type);
		j.at("functions").get_to(v.functions);
		j.at("types").get_to(v.types);
	}

	// NOLINTEND

	std::expected<DebugInfo, std::string> loadFromStream(std::istream& in) {
		try {
			const json j    = json::parse(in);
			DebugInfo  info = j.get<DebugInfo>();
			return info;
		} catch (const nlohmann::json::exception& e) {
			return std::unexpected(std::string(e.what()));
		}
	}

	void saveToStream(const DebugInfo& info, std::ostream& out) {
		const json j = info;
		out << j.dump(4);
	}

}  // namespace debug_info
