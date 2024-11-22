#include "key_spec_op.hpp"
#include <base/maps.hpp>
#include <base/exceptions.hpp>
#include <base/raw_view.hpp>
#include <base/init_guard.hpp>
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
	constexpr std::array<std::tuple<Keyword, std::string_view, KeywordFlag>, 66>
		lang_keywords_array{ {
			{ Keyword::Fun, "fun", KeywordFlag() },
			{ Keyword::Class, "class", KeywordFlag() },
			{ Keyword::Namespace, "namespace", KeywordFlag() },
			{ Keyword::Import, "import", KeywordFlag() },
			{ Keyword::As, "as", KeywordFlag() },
			{ Keyword::Using, "using", KeywordFlag() },
			{ Keyword::Alias, "alias", KeywordFlag() },
			{ Keyword::In, "in", KeywordFlag() },

			{ Keyword::Var, "var", KeywordFlag() },
			{ Keyword::Let, "let", KeywordFlag() },
			{ Keyword::Const, "const", KeywordFlag() },

			{ Keyword::While, "while", KeywordFlag() },
			{ Keyword::For, "for", KeywordFlag() },
			{ Keyword::Loop, "loop", KeywordFlag() },
			{ Keyword::If, "if", KeywordFlag() },
			{ Keyword::Else, "else", KeywordFlag() },
			{ Keyword::Elif, "elif", KeywordFlag() },

			{ Keyword::Block, "block", KeywordFlag() },
			{ Keyword::With, "with", KeywordFlag() },
			{ Keyword::Try, "try", KeywordFlag() },
			{ Keyword::Catch, "catch", KeywordFlag() },
			{ Keyword::Test, "test", KeywordFlag() },
			{ Keyword::Debug, "debug", KeywordFlag() },
			{ Keyword::Switch, "switch", KeywordFlag() },
			{ Keyword::Case, "case", KeywordFlag() },

			{ Keyword::Return, "return", KeywordFlags::IS_ACTION },
			{ Keyword::Break, "break", KeywordFlags::IS_ACTION },
			{ Keyword::Continue, "continue", KeywordFlags::IS_ACTION },
			{ Keyword::Redo, "redo", KeywordFlags::IS_ACTION },
			{ Keyword::Restart, "restart", KeywordFlags::IS_ACTION },
			{ Keyword::Defer, "defer", KeywordFlags::IS_ACTION },
			{ Keyword::Throw, "throw", KeywordFlags::IS_ACTION },
			{ Keyword::Assert, "assert", KeywordFlag() },
			{ Keyword::CompileAssert, "compile_assert", KeywordFlag() },

			{ Keyword::i8, "i8", KeywordFlag() },
			{ Keyword::i16, "i16", KeywordFlag() },
			{ Keyword::i32, "i32", KeywordFlag() },
			{ Keyword::i64, "i64", KeywordFlag() },
			{ Keyword::i128, "i128", KeywordFlag() },

			{ Keyword::u8, "u8", KeywordFlag() },
			{ Keyword::u16, "u16", KeywordFlag() },
			{ Keyword::u32, "u32", KeywordFlag() },
			{ Keyword::u64, "u64", KeywordFlag() },
			{ Keyword::u128, "u128", KeywordFlag() },

			{ Keyword::Float, "float", KeywordFlag() },
			{ Keyword::Char, "char", KeywordFlag() },
			{ Keyword::Bool, "bool", KeywordFlag() },

			{ Keyword::Vec, "vec", KeywordFlag() },
			{ Keyword::Set, "set", KeywordFlag() },
			{ Keyword::Dict, "dict", KeywordFlag() },
			{ Keyword::Array, "array", KeywordFlag() },

			{ Keyword::None, "none", KeywordFlag() },
			{ Keyword::True, "true", KeywordFlag() },
			{ Keyword::False, "false", KeywordFlag() },

			{ Keyword::Sizeof, "sizeof", KeywordFlag() },
			{ Keyword::Not, "not", KeywordFlag() },
			{ Keyword::And, "and", KeywordFlag() },
			{ Keyword::Or, "or", KeywordFlag() },
			{ Keyword::Xor, "xor", KeywordFlag() },

			{ Keyword::Extends, "extends", KeywordFlag() },
			{ Keyword::Implements, "implements", KeywordFlag() },
			{ Keyword::Public, "public", KeywordFlag() },
			{ Keyword::Private, "private", KeywordFlag() },
			{ Keyword::Protected, "protected", KeywordFlag() },
			{ Keyword::Static, "static", KeywordFlag() },
			{ Keyword::This, "this", KeywordFlag() },
		} };

	constexpr std::array<std::tuple<Keyword, std::string_view, KeywordFlag>, 17>
		bc_keywords_array{ {
			{ Keyword::BCFunction, "function", KeywordFlag() },
			{ Keyword::BCLocalSize, "local_size", KeywordFlag() },
			{ Keyword::BCRetSize, "ret_size", KeywordFlag() },
			{ Keyword::BCArgSize, "arg_size", KeywordFlag() },
			{ Keyword::BCNextArgSize, "next_arg_size", KeywordFlag() },
			{ Keyword::BCDefine, "define", KeywordFlag() },
			{ Keyword::BCLabel, "label", KeywordFlag() },
			{ Keyword::BCArg, "arg", KeywordFlag() },
			{ Keyword::BCCode, "code", KeywordFlag() },
			{ Keyword::BCType, "type", KeywordFlag() },
			{ Keyword::BCPrimitive, "primitive", KeywordFlag() },
			{ Keyword::BCPointer, "pointer", KeywordFlag() },
			{ Keyword::BCStaticTable, "static_table", KeywordFlag() },
			{ Keyword::BCDynamicTable, "dynamic_table", KeywordFlag() },
			{ Keyword::BCData, "data", KeywordFlag() },
			{ Keyword::BCVariant, "variant", KeywordFlag() },
			{ Keyword::BCFunType, "fun", KeywordFlag() },
		} };


	/**
	 * When modifing it modify also char.cpp -> makeCharTable
	 */
	constexpr std::array<std::pair<Special, std::string_view>, 6> special_array{ {
		{ Special::NotASpecial, "NotASpecial" },
		{ Special::Semicolon, ";" },
		{ Special::Comma, "," },
		{ Special::AtSign, "@" },
		{ Special::HashSign, "#" },
		{ Special::DolarSign, "$" },
	} };

	constexpr std::array<std::pair<Operator, std::string_view>, 15> operator_array{ {
		{ Operator::NotAnOperator, "NotAnOperator" },
		{ Operator::Period, "." },
		{ Operator::PeriodStar, ".*" },
		{ Operator::Colon, ":" },
		{ Operator::Assign, "=" },
		{ Operator::Pipe, "|" },
		{ Operator::QuestionMark, "?" },
		{ Operator::SingleArrow, "->" },
		{ Operator::DoubleArrow, "=>" },

		{ Operator::Minus, "-" },
		{ Operator::Plus, "+" },
		{ Operator::DoublePlus, "++" },
		{ Operator::DoubleMinus, "--" },
		{ Operator::Multiply, "*" },
		{ Operator::Divide, "/" },
	} };

	// Distinct for all keyword modes:
	base::VectorMap<base::StrID, Keyword, false, true> lang_keyword_map;
	base::VectorMap<base::StrID, Keyword, false, true> bc_keyword_map;

	// Single for all
	base::VectorMap<Keyword, base::StrID, false, true> rev_keyword_map;

	// Single for all:
	base::VectorMap<Keyword, KeywordFlag, false, true> keyword_flags;

	base::VectorMap<base::StrID, Special, false, true>  special_map;
	base::VectorMap<base::StrID, Operator, false, true> operator_map;

	base::VectorMap<Special, base::StrID, false, true>  rev_special_map;
	base::VectorMap<Operator, base::StrID, false, true> rev_operator_map;

	void key_spec_op::init() {
		SIMPLE_INIT_GUARD_BEGIN;

		keyword_mode = DEFAULT_MODE;

		// keywords:
		rev_keyword_map.put(Keyword::NotAKeyword, base::StrID("NotAKeyword"));
		keyword_flags.put(Keyword::NotAKeyword, KeywordFlag());

		for (auto [k, s, f]: lang_keywords_array) {
			lang_keyword_map.put(makeStrID(s), k);
			rev_keyword_map.put(k, makeStrID(s));
			keyword_flags.put(k, f);
		}

		for (auto [k, s, f]: bc_keywords_array) {
			bc_keyword_map.put(makeStrID(s), k);
			rev_keyword_map.put(k, makeStrID(s));
			keyword_flags.put(k, f);
		}

		// specials:
		for (auto [k, s]: special_array) {
			special_map.put(makeStrID(s), k);
			rev_special_map.put(k, makeStrID(s));
		}

		// operators:
		for (auto [k, s]: operator_array) {
			operator_map.put(makeStrID(s), k);
			rev_operator_map.put(k, makeStrID(s));
		}

		// just to be safe for any future changes
		// @TODO: move to same tests
		CORE_ASSERT(lang_keywords_array.size() == lang_keyword_map.size(), "keyword map error");
		CORE_ASSERT(
			special_array.size() == special_map.size()
				&& special_array.size() == rev_special_map.size(),
			"special map error"
		);
		CORE_ASSERT(
			operator_array.size() == operator_map.size()
				&& operator_array.size() == rev_operator_map.size(),
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

	Operator strAsOperator(base::StrID id) {
		if (operator_map.contains(id)) return operator_map[id];
		return Operator::NotAnOperator;
	}

	base::StrID keywordToStr(Keyword key) { return rev_keyword_map[key]; }

	base::StrID specialToStr(Special spec) { return rev_special_map[spec]; }

	base::StrID operatorToStr(Operator oper) { return rev_operator_map[oper]; }

	KeywordFlag keywordFlags(Keyword key) { return keyword_flags[key]; }

	std::vector<Keyword> getKeywords() {
		std::vector<Keyword> result;
		result.reserve(lang_keywords_array.size());
		for (const auto& [k, s, f]: lang_keywords_array) result.push_back(k);
		return result;
	}

	std::vector<Special> getSpecials() {
		std::vector<Special> result;
		result.reserve(special_array.size());
		for (const auto& [k, s]: special_array) result.push_back(k);
		return result;
	}

	std::vector<Operator> getOperators() {
		std::vector<Operator> result;
		result.reserve(operator_array.size());
		for (const auto& [k, s]: operator_array) result.push_back(k);
		return result;
	}
}
