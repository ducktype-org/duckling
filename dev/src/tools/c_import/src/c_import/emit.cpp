#include "emit.hpp"

#include <algorithm>
#include <format>
#include <ranges>

namespace c_import {

	namespace {

		constexpr std::string_view INDENT = "    ";

		std::string oneLine(std::string_view text) {
			std::string result{ text };
			std::ranges::replace(result, '\n', ' ');
			return result;
		}

		void appendBanner(std::string& out, const std::vector<std::string>& banner) {
			for (const auto& line: banner) out += std::format("# {}\n", oneLine(line));
			if (!banner.empty()) out += '\n';
		}

		std::string joinParams(const std::vector<DkNameType>& params) {
			std::string result;
			for (const auto& [index, param]: std::views::enumerate(params)) {
				if (index > 0) result += ", ";
				result += std::format("{}: {}", param.name, param.type);
			}
			return result;
		}

	}

	std::string emitModule(const DkModule& module, const std::vector<std::string>& banner) {
		std::string out;
		appendBanner(out, banner);

		if (!module.skipped.empty()) {
			out += "# Skipped declarations:\n";
			for (const auto& skipped: module.skipped)
				out += std::format("#   {}: {}\n", oneLine(skipped.name), oneLine(skipped.reason));
			out += '\n';
		}

		if (!module.classes.empty() || !module.fundecls.empty()) {
			out += "extern(\"C\") {\n";
			for (const auto& cls: module.classes) {
				if (!cls.comment.empty())
					out += std::format("{}# {}\n", INDENT, oneLine(cls.comment));
				out += std::format("{}class {} {{\n", INDENT, cls.name);
				for (const auto& field: cls.fields)
					out += std::format("{0}{0}{1}: {2};\n", INDENT, field.name, field.type);
				out += std::format("{}}}\n\n", INDENT);
			}
			for (const auto& decl: module.fundecls) {
				out += INDENT;
				if (decl.symbol_name)
					out += std::format("@c_symbol_name(\"{}\") ", *decl.symbol_name);
				out += std::format("fundecl {}({})", decl.name, joinParams(decl.params));
				if (decl.return_type) out += std::format(" -> {}", *decl.return_type);
				out += ";\n";
			}
			out += "}\n";
		}

		if (!module.constants.empty()) {
			out += '\n';
			for (const auto& constant: module.constants)
				out += std::format(
					"const {}: {} = {};\n", constant.name, constant.type, constant.value
				);
		}

		for (const auto& fun: module.functions) {
			out += std::format(
				"\nfun {}({}) -> {} = {{\n", fun.name, joinParams(fun.params), fun.return_type
			);
			for (const auto& line: fun.body) out += std::format("{}{}\n", INDENT, line);
			out += "}\n";
		}
		return out;
	}

	std::string emitLayoutCheck(
		const DkModule&                 module,
		const std::vector<std::string>& banner,
		const std::string&              bindings_import
	) {
		std::string out;
		appendBanner(out, banner);
		out += "import core.builtins.*;\n";
		out += std::format("import {}.*;\n\n", bindings_import);

		// @TODO: #3183 `size_of` and `alignment_of` are only exact in constant context.
		for (const auto& [index, layout]: std::views::enumerate(module.layouts)) {
			out += std::format("const C_LAYOUT_SIZE_{}: i64 = size_of({});\n", index, layout.name);
			out += std::format(
				"const C_LAYOUT_ALIGN_{}: i64 = alignment_of({});\n", index, layout.name
			);
		}

		out += "\nfun c_layout_mismatch() -> i64 = {\n";
		for (const auto& [index, layout]: std::views::enumerate(module.layouts)) {
			out += std::format(
				"{0}if (C_LAYOUT_SIZE_{1} != {2}i64) {{ return {3}i64; }}\n"
				"{0}if (C_LAYOUT_ALIGN_{1} != {4}i64) {{ return {3}i64; }}\n",
				INDENT,
				index,
				layout.size,
				index + 1,
				layout.align
			);
		}
		out += std::format("{}return 0i64;\n}}\n", INDENT);
		return out;
	}

}
