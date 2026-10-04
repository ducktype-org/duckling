#include "debug_info.hpp"

#include <base/config/build_type.hpp>
#include <base/except/exceptions.hpp>
#include <base/extend_cpp/variant_match.hpp>
#include <base/str/str_utils.hpp>

#include <sstream>
#include <variant>

namespace debug_info {

	namespace {

		SourcePosition resolvePosition(
			const SourcePosition&                                     pos,
			const std::function<FilePosition(const PstHashPostion&)>& resolver
		) {
			if (std::holds_alternative<FilePosition>(pos.line_col_position)) return pos;
			return SourcePosition{ .line_col_position
				                   = resolver(std::get<PstHashPostion>(pos.line_col_position)) };
		}

	}  // namespace

	void DebugInfo::resolvePositions(
		const std::function<FilePosition(const PstHashPostion&)>& resolver
	) {
		source_positions_type = SourcePositionsType::LineColumn;

		for (auto& [_, func]: functions) {
			if (func.position) func.position = resolvePosition(*func.position, resolver);

			for (auto& [_, parameter_meta]: func.parameter_indexes_to_metadata)
				if (parameter_meta.position)
					parameter_meta.position = resolvePosition(*parameter_meta.position, resolver);

			for (auto& [_, instr_meta]: func.instr_offsets_to_metadata)
				instr_meta.position = resolvePosition(instr_meta.position, resolver);

			for (auto& [_, variable_meta]: func.instr_offsets_to_variable_init)
				if (variable_meta.position)
					variable_meta.position = resolvePosition(*variable_meta.position, resolver);
		}
	}

	void DebugInfo::mergeFrom(DebugInfo&& other) {
		CORE_ASSERT(target == other.target, "Cannot merge debug info with different targets");
		CORE_ASSERT(
			source_positions_type == other.source_positions_type,
			"Cannot merge debug info with different source position types"
		);

		for (auto& [name, metadata]: other.functions) functions.emplace(name, std::move(metadata));
		for (auto& [name, metadata]: other.types) types.emplace(name, std::move(metadata));
		auto _ = std::move(other);
	}

	namespace {

		/**
		 * @brief The placeholders a dump needs: a position, an empty count and an invalid
		 * enumerator each print as a word rather than as nothing.
		 * @param position The position to print, if there is one.
		 * @return The position as text, or "<unknown>" when the Optional is empty.
		 */
		std::string orElse(const base::Optional<SourcePosition>& position) {
			return position.has_value() ? position.value().toString() : "<unknown>";
		}

		/**
		 * @brief Renders a count for a dump.
		 * @param count The number of entries.
		 * @return The count as text, or "none" when it is zero.
		 */
		std::string countOf(usize count) { return count == 0 ? "none" : std::to_string(count); }

		/**
		 * @brief Renders a Target for a dump.
		 * @return The enumerator's name, or "<invalid>" for a value outside the enum.
		 */
		std::string_view nameOf(Target target) {
			switch (target) {
			case Target::DBC:
				return "DBC";
			}
			return "<invalid>";
		}

		/**
		 * @brief Renders a SourcePositionsType for a dump.
		 * @return The enumerator's name, or "<invalid>" for a value outside the enum.
		 */
		std::string_view nameOf(SourcePositionsType type) {
			switch (type) {
			case SourcePositionsType::PstHash:
				return "PstHash";
			case SourcePositionsType::LineColumn:
				return "LineColumn";
			}
			return "<invalid>";
		}

	}  // namespace debug_info

	std::string SourcePosition::toString() const {
		variant_match(line_col_position) {
			variant_case(PstHashPostion, pst) {
				auto out = base::strConcat("pst[", pst.postion_scope_begin.toStringHex());
				if_opt_some(pst.postion_scope_end, end) out
					+= base::strConcat(" .. ", end.toStringHex());
				return out + "]";
			}
			variant_case(FilePosition, file) {
				return base::strConcat(
					file.file_path,
					":",
					file.start_line,
					":",
					file.start_column,
					" - ",
					file.end_line,
					":",
					file.end_column
				);
			}
		}
		CORE_PANIC("Unhandled SourcePosition alternative");
	}

	void DebugInfo::debugPrint(std::ostream& os) const {
		os << "DebugInfo {\n  target: " << nameOf(target) << "\n  module_path: "
		   << (module_path.empty() ? "<none>" : std::string_view(module_path))
		   << "\n  source_positions_type: " << nameOf(source_positions_type)
		   << "\n  functions: " << countOf(functions.size()) << "\n";

		for (const auto& [mangled_name, function]: functions) {
			os << "    function " << mangled_name << " {\n      name: "
			   << (function.function_name.has_value() ? std::string_view(*function.function_name)
			                                          : "<unnamed>")
			   << "\n      position: " << orElse(function.position) << "\n";

			// Parameters go by index, instructions and variable inits by bytecode offset.
			os << "      parameters: " << countOf(function.parameter_indexes_to_metadata.size())
			   << "\n";
			for (const auto& [index, parameter]: function.parameter_indexes_to_metadata)
				os << "        [" << index << "] " << parameter.name << " -> "
				   << orElse(parameter.position) << "\n";

			os << "      instructions: " << countOf(function.instr_offsets_to_metadata.size())
			   << "\n";
			for (const auto& [offset, instruction]: function.instr_offsets_to_metadata)
				os << "        @" << offset << " -> " << instruction.position.toString() << "\n";

			os << "      variable inits: "
			   << countOf(function.instr_offsets_to_variable_init.size()) << "\n";
			for (const auto& [offset, variable]: function.instr_offsets_to_variable_init)
				os << "        @" << offset << " " << variable.name << " -> "
				   << orElse(variable.position) << "\n";

			os << "    }\n";
		}

		os << "  types: " << countOf(types.size()) << "\n";
		for (const auto& [mangled_name, type]: types)
			os << "    " << mangled_name << " -> " << type.name << "\n";
		os << "}\n";
	}

	std::string DebugInfo::toString() const {
		std::ostringstream out;
		debugPrint(out);
		return out.str();
	}

}  // namespace debug_info
