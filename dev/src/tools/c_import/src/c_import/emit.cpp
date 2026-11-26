// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

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

		/// `doc` as comment lines indented by `indent`.
		void appendDoc(std::string& out, std::string_view indent, std::string_view doc) {
			if (doc.empty()) return;
			for (auto line: std::views::split(doc, '\n')) {
				std::string_view text{ line.begin(), line.end() };
				out += text.empty() ? std::format("{}#\n", indent)
				                    : std::format("{}# {}\n", indent, text);
			}
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
				appendDoc(out, INDENT, cls.doc);
				if (!cls.comment.empty())
					out += std::format("{}# {}\n", INDENT, oneLine(cls.comment));
				out += std::format("{}class {} {{\n", INDENT, cls.name);
				for (const auto& field: cls.fields) {
					appendDoc(out, std::format("{0}{0}", INDENT), field.doc);
					out += std::format("{0}{0}{1}: {2};\n", INDENT, field.name, field.type);
				}
				out += std::format("{}}}\n\n", INDENT);
			}
			for (const auto& [index, decl]: std::views::enumerate(module.fundecls)) {
				// A documented declaration is set apart from its neighbours.
				if (!decl.doc.empty() && index > 0) out += '\n';
				appendDoc(out, INDENT, decl.doc);
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
			for (const auto& [index, constant]: std::views::enumerate(module.constants)) {
				if ((!constant.doc.empty() || !constant.group_doc.empty()) && index > 0)
					out += '\n';
				// The enum's own doc stands apart from its first constant's.
				appendDoc(out, "", constant.group_doc);
				if (!constant.group_doc.empty()) out += '\n';
				appendDoc(out, "", constant.doc);
				out += std::format(
					"const {}: {} = {};\n", constant.name, constant.type, constant.value
				);
			}
		}

		for (const auto& fun: module.functions) {
			out += '\n';
			appendDoc(out, "", fun.doc);
			out += std::format(
				"fun {}({}) -> {} = {{\n", fun.name, joinParams(fun.params), fun.return_type
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
