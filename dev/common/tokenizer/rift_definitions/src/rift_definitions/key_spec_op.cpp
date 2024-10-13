#include "key_spec_op.hpp"
#include <base/maps.hpp>
#include <base/exceptions.hpp>
#include <base/raw_view.hpp>
#include <base/init_guard.hpp>
#include <array>

namespace rift_def {
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
	constexpr std::array<std::tuple<Keyword, std::string_view, base::FlagType>, 68>
		rift_keywords_array{ {
			{ Keyword::Fun, "fun", base::EmptyFlag },
			{ Keyword::Class, "class", base::EmptyFlag },
			{ Keyword::Namespace, "namespace", base::EmptyFlag },
			{ Keyword::Import, "import", base::EmptyFlag },
			{ Keyword::As, "as", base::EmptyFlag },
			{ Keyword::Using, "using", base::EmptyFlag },
			{ Keyword::Alias, "alias", base::EmptyFlag },
			{ Keyword::In, "in", base::EmptyFlag },
			{ Keyword::Lambda, "lambda", base::EmptyFlag },

			{ Keyword::Var, "var", base::EmptyFlag },
			{ Keyword::Let, "let", base::EmptyFlag },
			{ Keyword::Const, "const", base::EmptyFlag },

			{ Keyword::While, "while", base::EmptyFlag },
			{ Keyword::For, "for", base::EmptyFlag },
			{ Keyword::Loop, "loop", base::EmptyFlag },
			{ Keyword::If, "if", base::EmptyFlag },
			{ Keyword::Then, "then", base::EmptyFlag },
			{ Keyword::Else, "else", base::EmptyFlag },
			{ Keyword::Elif, "elif", base::EmptyFlag },

			{ Keyword::Block, "block", base::EmptyFlag },
			{ Keyword::With, "with", base::EmptyFlag },
			{ Keyword::Try, "try", base::EmptyFlag },
			{ Keyword::Catch, "catch", base::EmptyFlag },
			{ Keyword::Test, "test", base::EmptyFlag },
			{ Keyword::Debug, "debug", base::EmptyFlag },
			{ Keyword::Switch, "switch", base::EmptyFlag },
			{ Keyword::Case, "case", base::EmptyFlag },

			{ Keyword::Return, "return", KeywordFlags::is_action },
			{ Keyword::Break, "break", KeywordFlags::is_action },
			{ Keyword::Continue, "continue", KeywordFlags::is_action },
			{ Keyword::Redo, "redo", KeywordFlags::is_action },
			{ Keyword::Restart, "restart", KeywordFlags::is_action },
			{ Keyword::Defer, "defer", KeywordFlags::is_action },
			{ Keyword::Throw, "throw", KeywordFlags::is_action },
			{ Keyword::Assert, "assert", base::EmptyFlag },
			{ Keyword::CompileAssert, "compile_assert", base::EmptyFlag },

			{ Keyword::i8, "i8", base::EmptyFlag },
			{ Keyword::i16, "i16", base::EmptyFlag },
			{ Keyword::i32, "i32", base::EmptyFlag },
			{ Keyword::i64, "i64", base::EmptyFlag },
			{ Keyword::i128, "i128", base::EmptyFlag },

			{ Keyword::u8, "u8", base::EmptyFlag },
			{ Keyword::u16, "u16", base::EmptyFlag },
			{ Keyword::u32, "u32", base::EmptyFlag },
			{ Keyword::u64, "u64", base::EmptyFlag },
			{ Keyword::u128, "u128", base::EmptyFlag },

			{ Keyword::Float, "float", base::EmptyFlag },
			{ Keyword::Char, "char", base::EmptyFlag },
			{ Keyword::Bool, "bool", base::EmptyFlag },

			{ Keyword::Vec, "vec", base::EmptyFlag },
			{ Keyword::Set, "set", base::EmptyFlag },
			{ Keyword::Dict, "dict", base::EmptyFlag },
			{ Keyword::Array, "array", base::EmptyFlag },

			{ Keyword::None, "none", base::EmptyFlag },
			{ Keyword::True, "true", base::EmptyFlag },
			{ Keyword::False, "false", base::EmptyFlag },

			{ Keyword::Sizeof, "sizeof", base::EmptyFlag },
			{ Keyword::Not, "not", base::EmptyFlag },
			{ Keyword::And, "and", base::EmptyFlag },
			{ Keyword::Or, "or", base::EmptyFlag },
			{ Keyword::Xor, "xor", base::EmptyFlag },

			{ Keyword::Extends, "extends", base::EmptyFlag },
			{ Keyword::Implements, "implements", base::EmptyFlag },
			{ Keyword::Public, "public", base::EmptyFlag },
			{ Keyword::Private, "private", base::EmptyFlag },
			{ Keyword::Protected, "protected", base::EmptyFlag },
			{ Keyword::Static, "static", base::EmptyFlag },
			{ Keyword::This, "this", base::EmptyFlag },
		} };

	constexpr std::array<std::tuple<Keyword, std::string_view, base::FlagType>, 17>
		bc_keywords_array{ {
			{ Keyword::BCFunction, "function", base::EmptyFlag },
			{ Keyword::BCLocalSize, "local_size", base::EmptyFlag },
			{ Keyword::BCRetSize, "ret_size", base::EmptyFlag },
			{ Keyword::BCArgSize, "arg_size", base::EmptyFlag },
			{ Keyword::BCNextArgSize, "next_arg_size", base::EmptyFlag },
			{ Keyword::BCDefine, "define", base::EmptyFlag },
			{ Keyword::BCLabel, "label", base::EmptyFlag },
			{ Keyword::BCArg, "arg", base::EmptyFlag },
			{ Keyword::BCCode, "code", base::EmptyFlag },
			{ Keyword::BCType, "type", base::EmptyFlag },
			{ Keyword::BCPrimitive, "primitive", base::EmptyFlag },
			{ Keyword::BCPointer, "pointer", base::EmptyFlag },
			{ Keyword::BCStaticTable, "static_table", base::EmptyFlag },
			{ Keyword::BCDynamicTable, "dynamic_table", base::EmptyFlag },
			{ Keyword::BCData, "data", base::EmptyFlag },
			{ Keyword::BCVariant, "variant", base::EmptyFlag },
			{ Keyword::BCFunType, "fun", base::EmptyFlag },
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

	constexpr std::array<std::pair<NamedOperator, std::string_view>, 21> operator_array{ {
		{ NamedOperator::NotAnOperator, "NotAnOperator" },
		{ NamedOperator::Period, "." },
		{ NamedOperator::PeriodStar, ".*" },
		{ NamedOperator::Colon, ":" },
		{ NamedOperator::Assign, "=" },
		{ NamedOperator::Pipe, "|" },
		{ NamedOperator::QuestionMark, "?" },
		{ NamedOperator::SingleArrow, "->" },
		{ NamedOperator::DoubleArrow, "=>" },

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
	} };

	// Distinct for all keyword modes:
	base::VectorMap<base::StrID, Keyword, false, true> rift_keyword_map;
	base::VectorMap<base::StrID, Keyword, false, true> bc_keyword_map;

	// Single for all
	base::VectorMap<Keyword, base::StrID, false, true> rev_keyword_map;

	// Single for all:
	base::VectorMap<Keyword, base::FlagType, false, true> keyword_flags;

	base::VectorMap<base::StrID, Special, false, true>  special_map;
	base::VectorMap<base::StrID, NamedOperator, false, true> operator_map;

	base::VectorMap<Special, base::StrID, false, true>  rev_special_map;
	base::VectorMap<NamedOperator, base::StrID, false, true> rev_operator_map;

	void key_spec_op::init() {
		RIFT_SIMPLE_INIT_GUARD_BEGIN;

		keyword_mode = DEFAULT_MODE;

		// keywords:
		rev_keyword_map.put(Keyword::NotAKeyword, base::StrID("NotAKeyword"));
		keyword_flags.put(Keyword::NotAKeyword, base::EmptyFlag);

		for (auto [k, s, f]: rift_keywords_array) {
			rift_keyword_map.put(makeStrID(s), k);
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
		RIFT_ASSERT(rift_keywords_array.size() == rift_keyword_map.size(), "keyword map error");
		RIFT_ASSERT(
			special_array.size() == special_map.size()
				&& special_array.size() == rev_special_map.size(),
			"special map error"
		);
		RIFT_ASSERT(
			operator_array.size() == operator_map.size()
				&& operator_array.size() == rev_operator_map.size(),
			"operator map error"
		);

		RIFT_SIMPLE_INIT_GUARD_END;
	}

	Keyword strAsKeyword(base::StrID id) {
		switch (keyword_mode) {
		case KeywordMode::RiftSource:
			if (rift_keyword_map.contains(id))
				return rift_keyword_map[id];
			else
				return Keyword::NotAKeyword;

		case KeywordMode::RiftBC:
			if (bc_keyword_map.contains(id))
				return bc_keyword_map[id];
			else
				return Keyword::NotAKeyword;

		default:
			RIFT_PANIC("Illegal keyword_mode");
		}
		if (rift_keyword_map.contains(id)) return rift_keyword_map[id];
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

	base::FlagType keywordFlags(Keyword key) { return keyword_flags[key]; }

	std::vector<Keyword> getKeywords() {
		std::vector<Keyword> result;
		result.reserve(rift_keywords_array.size());
		for (const auto& [k, s, f]: rift_keywords_array) result.push_back(k);
		return result;
	}

	std::vector<Special> getSpecials() {
		std::vector<Special> result;
		result.reserve(special_array.size());
		for (const auto& [k, s]: special_array) result.push_back(k);
		return result;
	}

	std::vector<NamedOperator> getOperators() {
		std::vector<NamedOperator> result;
		result.reserve(operator_array.size());
		for (const auto& [k, s]: operator_array) result.push_back(k);
		return result;
	}
}
