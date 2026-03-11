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
}  // namespace nlohmann

namespace debug_info {
	using json = nlohmann::json;

	// -- PstHash

	inline void to_json(json& j, const PstHash& v) {
		j = json{ { "a", v.a }, { "b", v.b }, { "c", v.c }, { "d", v.d } };
	}

	inline void from_json(const json& j, PstHash& v) {
		j.at("a").get_to(v.a);
		j.at("b").get_to(v.b);
		j.at("c").get_to(v.c);
		j.at("d").get_to(v.d);
	}

	// -- Optional

	template<typename T>
	inline void to_json(json& j, const base::Optional<T>& opt) {
		if (opt)
			j = *opt;
		else
			j = nullptr;
	}

	template<typename T>
	inline void from_json(const json& j, base::Optional<T>& opt) {
		if (j.is_null())
			opt = std::nullopt;
		else
			opt = j.get<T>();
	}

	// -- PstHashPostion

	inline void to_json(json& j, const PstHashPostion& v) {
		j = json{ { "position_scope_begin", v.postion_scope_begin },
			      { "position_scope_end", v.postion_scope_end } };
	}

	inline void from_json(const json& j, PstHashPostion& v) {
		j.at("position_scope_begin").get_to(v.postion_scope_begin);
		if (j.contains("position_scope_end"))
			j.at("position_scope_end").get_to(v.postion_scope_end);
	}

	// -- FilePosition

	inline void to_json(json& j, const FilePosition& v) {
		j = json{ { "file_path", v.file_path },
			      { "start_line", v.start_line },
			      { "start_column", v.start_column },
			      { "end_line", v.end_line },
			      { "end_column", v.end_column } };
	}

	inline void from_json(const json& j, FilePosition& v) {
		j.at("file_path").get_to(v.file_path);
		j.at("start_line").get_to(v.start_line);
		j.at("start_column").get_to(v.start_column);
		j.at("end_line").get_to(v.end_line);
		j.at("end_column").get_to(v.end_column);
	}

	// -- SourcePosition

	inline void to_json(json& j, const SourcePosition& v) {
		if (std::holds_alternative<PstHashPostion>(v.line_col_position)) {
			j = { { "type", "PstHashPosition" },
				  { "value", std::get<PstHashPostion>(v.line_col_position) } };
		} else {
			j = { { "type", "FilePosition" },
				  { "value", std::get<FilePosition>(v.line_col_position) } };
		}
	}

	inline void from_json(const json& j, SourcePosition& v) {
		const std::string type = j.at("type");
		if (type == "PstHashPosition")
			v.line_col_position = j.at("value").get<PstHashPostion>();
		else if (type == "FilePosition")
			v.line_col_position = j.at("value").get<FilePosition>();
	}

	// -- InstructionMetadata

	inline void to_json(json& j, const InstructionMetadata& v) {
		j = json{ { "position", v.position } };
	}

	inline void from_json(const json& j, InstructionMetadata& v) {
		j.at("position").get_to(v.position);
	}

	// -- TypeMetadata

	inline void to_json(json& j, const TypeMetadata& v) { j = json{ { "name", v.name } }; }

	inline void from_json(const json& j, TypeMetadata& v) { j.at("name").get_to(v.name); }

	// -- FunctionMetadata

	inline void to_json(json& j, const FunctionMetadata& v) {
		// Sort a temporary copy by offset so the on-disk format is always ordered.
		auto sorted = v.instr_offsets_to_metadata;
		std::ranges::sort(sorted, {}, &decltype(sorted)::value_type::first);

		j = json{ { "function_name", v.function_name },
			      { "position", v.position },
			      { "instr_offsets_to_metadata", sorted } };
	}

	inline void from_json(const json& j, FunctionMetadata& v) {
		j.at("function_name").get_to(v.function_name);
		j.at("position").get_to(v.position);
		j.at("instr_offsets_to_metadata").get_to(v.instr_offsets_to_metadata);
		if (!std::ranges::is_sorted(
				v.instr_offsets_to_metadata,
				{},
				&decltype(v.instr_offsets_to_metadata)::value_type::first
			))
			throw nlohmann::json::other_error::create(
				501, "instr_offsets_to_metadata is not sorted by offset", &j
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

	inline void to_json(json& j, const DebugInfo& v) {
		j = json{ { "target", v.target },
			      { "module_path", v.module_path },
			      { "source_positions_type", v.source_positions_type },
			      { "functions", v.functions },
			      { "types", v.types } };
	}

	inline void from_json(const json& j, DebugInfo& v) {
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
