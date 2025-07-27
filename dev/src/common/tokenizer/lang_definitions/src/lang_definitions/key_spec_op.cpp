#include "key_spec_op.hpp"

#include <base/exceptions.hpp>
#include <base/init_guard.hpp>
#include <base/maps.hpp>
#include <base/raw_view.hpp>

#include <array>

namespace lang_def {
	namespace {
		KeywordMode keyword_mode;
	}

	void setKeywordMode(KeywordMode mode) { keyword_mode = mode; }

	base::StrID makeStrID(std::string_view view) {
		return base::StrID(base::RawView({ reinterpret_cast<const byte*>(view.data()), view.size() }
		));
	}

	// @TODO: what if there are many instances of one keyword (vec and vector)
	// @TODO: shouldn't types such as vec, dict be Vec, Dict...
	constexpr auto LANG_KEYWORDS_ARRAY
		= std::to_array<std::tuple<Keyword, std::string_view, KeywordFlags>>({
			{ Keyword::Fun, "fun", KeywordFlags() },
			{ Keyword::Class, "class", KeywordFlags() },
			{ Keyword::Namespace, "namespace", KeywordFlags() },
			{ Keyword::Import, "import", KeywordFlags() },
			{ Keyword::As, "as", KeywordFlags() },
			{ Keyword::Using, "using", KeywordFlags() },
			{ Keyword::Alias, "alias", KeywordFlags() },
			{ Keyword::In, "in", KeywordFlags() },
			{ Keyword::Lambda, "lambda", KeywordFlags() },
			{ Keyword::Var, "var", KeywordFlags() },
			{ Keyword::Let, "let", KeywordFlags() },
			{ Keyword::Const, "const", KeywordFlagsOptions::IsGenPrefixOp },

			{ Keyword::While, "while", KeywordFlags() },
			{ Keyword::For, "for", KeywordFlags() },
			{ Keyword::Loop, "loop", KeywordFlags() },
			{ Keyword::If, "if", KeywordFlags() },
			{ Keyword::Then, "then", KeywordFlags() },
			{ Keyword::Else, "else", KeywordFlags() },
			{ Keyword::Elif, "elif", KeywordFlags() },
			{ Keyword::Block, "block", KeywordFlags() },
			{ Keyword::With, "with", KeywordFlags() },
			{ Keyword::Try, "try", KeywordFlags() },
			{ Keyword::Catch, "catch", KeywordFlags() },
			{ Keyword::Test, "test", KeywordFlags() },
			{ Keyword::Debug, "debug", KeywordFlags() },
			{ Keyword::Switch, "switch", KeywordFlags() },
			{ Keyword::Case, "case", KeywordFlags() },

			{ Keyword::Return, "return", KeywordFlagsOptions::IsAction },
			{ Keyword::Break, "break", KeywordFlagsOptions::IsAction },
			{ Keyword::Continue, "continue", KeywordFlagsOptions::IsAction },
			{ Keyword::Redo, "redo", KeywordFlagsOptions::IsAction },
			{ Keyword::Restart, "restart", KeywordFlagsOptions::IsAction },
			{ Keyword::Defer, "defer", KeywordFlagsOptions::IsAction },
			{ Keyword::Throw, "throw", KeywordFlagsOptions::IsAction },
			{ Keyword::Assert, "assert", KeywordFlags() },
			{ Keyword::CompileAssert, "compile_assert", KeywordFlags() },

			{ Keyword::Expand, "expand", KeywordFlags() },

			{ Keyword::i8, "i8", KeywordFlags() },
			{ Keyword::i16, "i16", KeywordFlags() },
			{ Keyword::i32, "i32", KeywordFlags() },
			{ Keyword::i64, "i64", KeywordFlags() },
			{ Keyword::i128, "i128", KeywordFlags() },

			{ Keyword::u8, "u8", KeywordFlags() },
			{ Keyword::u16, "u16", KeywordFlags() },
			{ Keyword::u32, "u32", KeywordFlags() },
			{ Keyword::u64, "u64", KeywordFlags() },
			{ Keyword::u128, "u128", KeywordFlags() },

			{ Keyword::f16, "f16", KeywordFlags() },
			{ Keyword::f32, "f32", KeywordFlags() },
			{ Keyword::f64, "f64", KeywordFlags() },
			{ Keyword::f80, "f80", KeywordFlags() },
			{ Keyword::f128, "f128", KeywordFlags() },
			{ Keyword::Char, "char", KeywordFlags() },
			{ Keyword::Bool, "bool", KeywordFlags() },
			{ Keyword::Str, "str", KeywordFlags() },

			{ Keyword::Vec, "vec", KeywordFlags() },
			{ Keyword::Set, "set", KeywordFlags() },
			{ Keyword::Dict, "dict", KeywordFlags() },
			{ Keyword::Array, "array", KeywordFlags() },

			{ Keyword::None, "none", KeywordFlags() },
			{ Keyword::True, "true", KeywordFlags() },
			{ Keyword::False, "false", KeywordFlags() },

			{ Keyword::Sizeof, "sizeof", KeywordFlags() },
			{ Keyword::Not, "not", KeywordFlags() },
			{ Keyword::And, "and", KeywordFlags() },
			{ Keyword::Or, "or", KeywordFlags() },
			{ Keyword::Xor, "xor", KeywordFlags() },

			{ Keyword::Ref, "ref", KeywordFlagsOptions::IsGenPrefixOp },
			{ Keyword::Box, "box", KeywordFlagsOptions::IsGenPrefixOp },
			{ Keyword::Copy, "copy", KeywordFlagsOptions::IsGenPrefixOp },
			{ Keyword::Move, "move", KeywordFlagsOptions::IsGenPrefixOp },
			{ Keyword::Refof, "refof", KeywordFlagsOptions::IsGenPrefixOp },

			{ Keyword::Extends, "extends", KeywordFlags() },
			{ Keyword::Implements, "implements", KeywordFlags() },
			{ Keyword::Public, "public", KeywordFlags() },
			{ Keyword::Private, "private", KeywordFlags() },
			{ Keyword::Protected, "protected", KeywordFlags() },
			{ Keyword::Static, "static", KeywordFlags() },
			{ Keyword::This, "this", KeywordFlags() },
		});

	constexpr auto BC_KEYWORDS_ARRAY
		= std::to_array<std::tuple<Keyword, std::string_view, KeywordFlags>>({
			{ Keyword::BCFunction, "function", KeywordFlags() },
			{ Keyword::BCType, "type", KeywordFlags() },
			{ Keyword::BCPrimitive, "primitive", KeywordFlags() },
			{ Keyword::BCPointer, "pointer", KeywordFlags() },
			{ Keyword::BCFixedSizeTable, "fixed_size_table", KeywordFlags() },
			{ Keyword::BCDynamicTable, "dynamic_table", KeywordFlags() },
			{ Keyword::BCData, "data", KeywordFlags() },
			{ Keyword::BCVariant, "variant", KeywordFlags() },
			{ Keyword::BCFunType, "fun", KeywordFlags() },
			{ Keyword::BCGlobalData, "global_data", KeywordFlags() },
			{ Keyword::BCGlobalConstructor, "constructor", KeywordFlags() },
			{ Keyword::BCGlobalDestructor, "destructor", KeywordFlags() },
			{ Keyword::BCOpaque, "opaque", KeywordFlags() },
			{ Keyword::BCClass, "class", KeywordFlags() },
			{ Keyword::BCAbstract, "abstract", KeywordFlags() },
			{ Keyword::BCInterface, "interface", KeywordFlags() },
			{ Keyword::BCExtends, "extends", KeywordFlags() },
			{ Keyword::BCImplements, "implements", KeywordFlags() },
			{ Keyword::BCVirtualMethods, "virtual_methods", KeywordFlags() },
			{ Keyword::BCFields, "fields", KeywordFlags() },
			{ Keyword::BCMethodImplementations, "implementations", KeywordFlags() },
			{ Keyword::BCTrue, "true", KeywordFlags() },
			{ Keyword::BCFalse, "false", KeywordFlags() },
		});

	// `- 1` because of `Keyword::NotAKeyword`
	static_assert(
		static_cast<usize>(Keyword::COUNT) - 1
		== LANG_KEYWORDS_ARRAY.size() + BC_KEYWORDS_ARRAY.size()
	);

	/**
	 * When modifing it modify also char.cpp -> makeCharTable
	 */
	constexpr auto SPECIAL_ARRAY = std::to_array<std::pair<Special, std::string_view>>({
		{ Special::NotASpecial, "NotASpecial" },
		{ Special::Semicolon, ";" },
		{ Special::Comma, "," },
		{ Special::AtSign, "@" },
		{ Special::HashSign, "#" },
		{ Special::DollarSign, "$" },
	});

	constexpr auto OPERATOR_ARRAY = std::to_array<std::pair<NamedOperator, std::string_view>>({
		{ NamedOperator::NotAnOperator, "NotAnOperator" },

		{ NamedOperator::Period, "." },
		{ NamedOperator::PeriodStar, ".*" },
		{ NamedOperator::Colon, ":" },
		{ NamedOperator::Assign, "=" },
		{ NamedOperator::QuestionMark, "?" },
		{ NamedOperator::SingleArrow, "->" },
		{ NamedOperator::DoubleArrow, "=>" },

		{ NamedOperator::Pipe, "|" },
		{ NamedOperator::BitAnd, "&" },
		{ NamedOperator::BitXor, "^" },

		{ NamedOperator::LeftShift, "<<" },
		{ NamedOperator::RightShift, ">>" },

		{ NamedOperator::Lesser, "<" },
		{ NamedOperator::Greater, ">" },
		{ NamedOperator::LEqual, "<=" },
		{ NamedOperator::GEqual, ">=" },
		{ NamedOperator::Equal, "==" },
		{ NamedOperator::NotEqual, "!=" },

		{ NamedOperator::Minus, "-" },
		{ NamedOperator::Plus, "+" },
		{ NamedOperator::DoublePlus, "++" },
		{ NamedOperator::DoubleMinus, "--" },
		{ NamedOperator::Multiply, "*" },
		{ NamedOperator::Divide, "/" },
		{ NamedOperator::Remainder, "%" },
		{ NamedOperator::Exponentiate, "**" },
	});

	// Distinct for all keyword modes:
	base::VectorMap<base::StrID, Keyword, false, true> lang_keyword_map;
	base::VectorMap<base::StrID, Keyword, false, true> bc_keyword_map;

	// Single for all
	base::VectorMap<Keyword, base::StrID, false, true> rev_keyword_map;

	// Single for all:
	base::VectorMap<Keyword, KeywordFlags, false, true> keyword_flags;

	base::VectorMap<base::StrID, Special, false, true>       special_map;
	base::VectorMap<base::StrID, NamedOperator, false, true> operator_map;

	base::VectorMap<Special, base::StrID, false, true>       rev_special_map;
	base::VectorMap<NamedOperator, base::StrID, false, true> rev_operator_map;

	void key_spec_op::init() {
		SIMPLE_INIT_GUARD_BEGIN;

		keyword_mode = DEFAULT_MODE;

		// keywords:
		rev_keyword_map.put(Keyword::NotAKeyword, base::StrID("NotAKeyword"));
		keyword_flags.put(Keyword::NotAKeyword, KeywordFlags());

		for (auto [k, s, f]: LANG_KEYWORDS_ARRAY) {
			lang_keyword_map.put(makeStrID(s), k);
			rev_keyword_map.put(k, makeStrID(s));
			keyword_flags.put(k, f);
		}

		for (auto [k, s, f]: BC_KEYWORDS_ARRAY) {
			bc_keyword_map.put(makeStrID(s), k);
			rev_keyword_map.put(k, makeStrID(s));
			keyword_flags.put(k, f);
		}

		// specials:
		for (auto [k, s]: SPECIAL_ARRAY) {
			special_map.put(makeStrID(s), k);
			rev_special_map.put(k, makeStrID(s));
		}

		// operators:
		for (auto [k, s]: OPERATOR_ARRAY) {
			operator_map.put(makeStrID(s), k);
			rev_operator_map.put(k, makeStrID(s));
		}

		// just to be safe for any future changes
		// @TODO: move to same tests
		CORE_ASSERT(LANG_KEYWORDS_ARRAY.size() == lang_keyword_map.size(), "keyword map error");
		CORE_ASSERT(
			SPECIAL_ARRAY.size() == special_map.size()
				&& SPECIAL_ARRAY.size() == rev_special_map.size(),
			"special map error"
		);
		CORE_ASSERT(
			OPERATOR_ARRAY.size() == operator_map.size()
				&& OPERATOR_ARRAY.size() == rev_operator_map.size(),
			"operator map error"
		);

		SIMPLE_INIT_GUARD_END;
	}

	Keyword strAsKeyword(base::StrID id) {
		switch (keyword_mode) {
		case KeywordMode::DucklingSource:
			if (lang_keyword_map.contains(id))
				return lang_keyword_map[id];
			else
				return Keyword::NotAKeyword;

		case KeywordMode::DuckBC:
			if (bc_keyword_map.contains(id))
				return bc_keyword_map[id];
			else
				return Keyword::NotAKeyword;

		default:
			CORE_PANIC("Illegal keyword_mode");
		}
		if (lang_keyword_map.contains(id)) return lang_keyword_map[id];
		return Keyword::NotAKeyword;
	}

	Special strAsSpecial(base::StrID id) {
		if (special_map.contains(id)) return special_map[id];
		return Special::NotASpecial;
	}

	NamedOperator strAsOperator(base::StrID id) {
		if (operator_map.contains(id)) return operator_map[id];
		return NamedOperator::NotAnOperator;
	}

	base::StrID keywordToStr(Keyword key) { return rev_keyword_map[key]; }

	base::StrID specialToStr(Special spec) { return rev_special_map[spec]; }

	base::StrID operatorToStr(NamedOperator oper) { return rev_operator_map[oper]; }

	KeywordFlags keywordFlags(Keyword key) { return keyword_flags[key]; }

	std::vector<Keyword> getKeywords() {
		std::vector<Keyword> result;
		result.reserve(LANG_KEYWORDS_ARRAY.size());
		for (const auto& [k, s, f]: LANG_KEYWORDS_ARRAY) result.push_back(k);
		return result;
	}

	std::vector<Special> getSpecials() {
		std::vector<Special> result;
		result.reserve(SPECIAL_ARRAY.size());
		for (const auto& [k, s]: SPECIAL_ARRAY) result.push_back(k);
		return result;
	}

	std::vector<NamedOperator> getOperators() {
		std::vector<NamedOperator> result;
		result.reserve(OPERATOR_ARRAY.size());
		for (const auto& [k, s]: OPERATOR_ARRAY) result.push_back(k);
		return result;
	}
}
