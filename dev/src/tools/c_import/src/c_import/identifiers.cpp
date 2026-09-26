#include "identifiers.hpp"

#include <string>
#include <unordered_set>

namespace c_import {

	namespace {

		// Mirrors the keyword table in
		// src/common/tokenizer/lang_definitions/src/lang_definitions/key_spec_op.cpp
		const std::unordered_set<std::string>& keywords() {
			static const std::unordered_set<std::string> table = {
				"Array",     "Dict",
				"Set",       "and",
				"assert",    "block",
				"bool",      "box",
				"break",     "case",
				"catch",     "char",
				"class",     "compile_assert",
				"const",     "continue",
				"copy",      "copyof",
				"cptr",      "debug",
				"defer",     "destroy",
				"else",      "expand",
				"export",    "extends",
				"extern",    "f128",
				"f16",       "f32",
				"f64",       "f80",
				"false",     "ffi",
				"for",       "fun",
				"function",  "fundecl",
				"hides",     "i128",
				"i16",       "i32",
				"i64",       "i8",
				"if",        "implements",
				"import",    "in",
				"lambda",    "let",
				"loop",      "manyptr",
				"match",     "move",
				"namespace", "new",
				"none",      "not",
				"object",    "or",
				"pattern",   "private",
				"protected", "ptr",
				"ptrof",     "public",
				"redo",      "ref",
				"refof",     "restart",
				"return",    "self",
				"sizeof",    "slice",
				"static",    "str",
				"switch",    "template",
				"then",      "throw",
				"true",      "try",
				"type",      "u128",
				"u16",       "u32",
				"u64",       "u8",
				"using",     "var",
				"void",      "while",
				"with",      "xor",
			};
			return table;
		}

	}

	bool isDucklingKeyword(std::string_view name) { return keywords().contains(std::string(name)); }

	std::string taggedRecordName(std::string_view tag) { return "struct_" + std::string(tag); }

	std::string taggedUnionName(std::string_view tag) { return "union_" + std::string(tag); }

	std::string taggedEnumName(std::string_view tag) { return "enum_" + std::string(tag); }

	NameRegistry::Verdict NameRegistry::claim(std::string_view name) {
		if (name.empty()) return { .accepted = false, .reason = "the declaration has no name" };

		if (isDucklingKeyword(name))
			return { .accepted = false,
				     .reason   = "`" + std::string(name) + "` is a Duckling keyword" };

		auto [_, inserted] = m_claimed.emplace(std::string(name), true);
		if (!inserted)
			return { .accepted = false,
				     .reason   = "`" + std::string(name) + "` was already emitted" };

		return { .accepted = true, .reason = {} };
	}

	bool NameRegistry::isClaimed(std::string_view name) const {
		return m_claimed.contains(std::string(name));
	}

}
