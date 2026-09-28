#include "key_spec_op.hpp"

#include <base/collections/maps.hpp>
#include <base/except/exceptions.hpp>
#include <base/misc/init_guard.hpp>
#include <base/misc/raw_view.hpp>

#include <array>

namespace lang_def {
	namespace {
		/**
		 * Global tokenization state.
		 *
		 * @note This is thread local, because workers tokenize code in parallel, so a shared global
		 * here would be a data race. It can be thread-local because the mode is always set before
		 * tokenization.
		 *
		 * @TODO: #2943 remove this global state
		 */
		thread_local KeywordMode keyword_mode = DEFAULT_MODE;
	}

	void setKeywordMode(KeywordMode mode) { keyword_mode = mode; }

	KeywordMode getKeywordMode() { return keyword_mode; }

	base::StrID makeStrID(std::string_view view) {
		return base::StrID(base::RawView({ std::as_bytes(std::span{ view }) }));
	}

	// @TODO: what if there are many instances of one keyword (vec and vector)
	constexpr auto LANG_KEYWORDS_ARRAY
		= std::to_array<std::tuple<Keyword, std::string_view, KeywordFlags>>({
			// These are Keywords that should always indicate a start of a statement.
			// This allows for better handling of bad parsing cases.
			{ Keyword::Fun, "fun", KeywordFlagsOptions::IsStmtStart },
			{ Keyword::FunDecl, "fundecl", KeywordFlagsOptions::IsStmtStart },
			{ Keyword::Pattern, "pattern", KeywordFlagsOptions::IsStmtStart },
			{ Keyword::Class, "class", KeywordFlagsOptions::IsStmtStart },
			{ Keyword::Namespace, "namespace", KeywordFlagsOptions::IsStmtStart },
			{ Keyword::Import, "import", KeywordFlagsOptions::IsStmtStart },
			{ Keyword::Using, "using", KeywordFlagsOptions::IsStmtStart },
			{ Keyword::Var, "var", KeywordFlagsOptions::IsStmtStart },
			{ Keyword::Let, "let", KeywordFlagsOptions::IsStmtStart },
			{ Keyword::While, "while", KeywordFlagsOptions::IsStmtStart },
			{ Keyword::For, "for", KeywordFlagsOptions::IsStmtStart },
			{ Keyword::Loop, "loop", KeywordFlagsOptions::IsStmtStart },
			{ Keyword::Block, "block", KeywordFlagsOptions::IsStmtStart },
			{ Keyword::Expand, "expand", KeywordFlagsOptions::IsStmtStart },

			// @note: Template is a bit special, it acts more as a specifier so it being a stmt
			// start might not always be what we want.
			{ Keyword::Template, "template", KeywordFlagsOptions::IsStmtStart },

			// These Keywords also indicate start of a statement.
			{ Keyword::Return,
	          "return",
	          KeywordFlagsOptions::IsStmtStart | KeywordFlagsOptions::IsAction },
			{ Keyword::Break,
	          "break",
	          KeywordFlagsOptions::IsStmtStart | KeywordFlagsOptions::IsAction },
			{ Keyword::Continue,
	          "continue",
	          KeywordFlagsOptions::IsStmtStart | KeywordFlagsOptions::IsAction },
			{ Keyword::Redo,
	          "redo",
	          KeywordFlagsOptions::IsStmtStart | KeywordFlagsOptions::IsAction },
			{ Keyword::Restart,
	          "restart",
	          KeywordFlagsOptions::IsStmtStart | KeywordFlagsOptions::IsAction },
			{ Keyword::Defer,
	          "defer",
	          KeywordFlagsOptions::IsStmtStart | KeywordFlagsOptions::IsAction },
			{ Keyword::Throw,
	          "throw",
	          KeywordFlagsOptions::IsStmtStart | KeywordFlagsOptions::IsAction },

			// These Keywords also indicate start of a statement.
			{ Keyword::Public, "public", KeywordFlagsOptions::IsSpecifier },
			{ Keyword::Private, "private", KeywordFlagsOptions::IsSpecifier },
			{ Keyword::Protected, "protected", KeywordFlagsOptions::IsSpecifier },
			{ Keyword::Extern, "extern", KeywordFlagsOptions::IsSpecifier },
			{ Keyword::Debug, "debug", KeywordFlagsOptions::IsSpecifier },
			{ Keyword::Static, "static", KeywordFlagsOptions::IsSpecifier },
			{ Keyword::Export, "export", KeywordFlagsOptions::IsSpecifier },

			// If doesn't always indicate statement start.
			{ Keyword::If, "if", KeywordFlags() },
			{ Keyword::Then, "then", KeywordFlags() },
			{ Keyword::Else, "else", KeywordFlags() },

			// This is the list of keywords that are general prefix operators
			{ Keyword::Const, "const", KeywordFlagsOptions::IsGenPrefixOp },
			{ Keyword::Ref, "ref", KeywordFlagsOptions::IsGenPrefixOp },
			{ Keyword::Box, "box", KeywordFlagsOptions::IsGenPrefixOp },
			{ Keyword::New, "new", KeywordFlagsOptions::IsGenPrefixOp },
			{ Keyword::Ptr, "ptr", KeywordFlagsOptions::IsGenPrefixOp },
			{ Keyword::CPtr, "cptr", KeywordFlagsOptions::IsGenPrefixOp },
			{ Keyword::ManyPtr, "manyptr", KeywordFlagsOptions::IsGenPrefixOp },
			{ Keyword::Slice, "slice", KeywordFlagsOptions::IsGenPrefixOp },
			{ Keyword::Copy, "copy", KeywordFlagsOptions::IsGenPrefixOp },
			{ Keyword::Copyof, "copyof", KeywordFlagsOptions::IsGenPrefixOp },
			{ Keyword::Move, "move", KeywordFlagsOptions::IsGenPrefixOp },
			{ Keyword::Refof, "refof", KeywordFlagsOptions::IsGenPrefixOp },
			{ Keyword::Ptrof, "ptrof", KeywordFlagsOptions::IsGenPrefixOp },

			// `not` isn't a general prefix operator,
			// it has specific handling together with the other boolean operators
			{ Keyword::Not, "not", KeywordFlags() },

			{ Keyword::And, "and", KeywordFlags() },
			{ Keyword::Or, "or", KeywordFlags() },
			{ Keyword::Xor, "xor", KeywordFlags() },

			{ Keyword::Hides, "hides", KeywordFlags() },
			{ Keyword::In, "in", KeywordFlags() },
			{ Keyword::Lambda, "lambda", KeywordFlags() },

			{ Keyword::With, "with", KeywordFlags() },
			{ Keyword::Try, "try", KeywordFlags() },
			{ Keyword::Catch, "catch", KeywordFlags() },
			{ Keyword::Match, "match", KeywordFlags() },
			{ Keyword::Switch, "switch", KeywordFlags() },
			{ Keyword::Case, "case", KeywordFlags() },

			{ Keyword::Assert, "assert", KeywordFlags() },
			{ Keyword::CompileAssert, "compile_assert", KeywordFlags() },

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
			{ Keyword::Type, "type", KeywordFlags() },
			{ Keyword::Void, "void", KeywordFlags() },

			{ Keyword::Set, "Set", KeywordFlags() },
			{ Keyword::Dict, "Dict", KeywordFlags() },
			{ Keyword::Array, "Array", KeywordFlags() },

			{ Keyword::None, "none", KeywordFlags() },
			{ Keyword::True, "true", KeywordFlags() },
			{ Keyword::False, "false", KeywordFlags() },

			{ Keyword::Sizeof, "sizeof", KeywordFlags() },

			{ Keyword::Extends, "extends", KeywordFlags() },
			{ Keyword::Implements, "implements", KeywordFlags() },
			{ Keyword::Self, "self", KeywordFlags() },

			{ Keyword::Destroy, "destroy", KeywordFlags() },
		});

	constexpr auto BC_KEYWORDS_ARRAY
		= std::to_array<std::tuple<Keyword, std::string_view, KeywordFlags>>({
			{ Keyword::BCFunction, "function", KeywordFlags() },
			{ Keyword::BCFfi, "ffi", KeywordFlags() },
			{ Keyword::BCObject, "object", KeywordFlags() },
			{ Keyword::BCAssertSize, "assert_size", KeywordFlags() },
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
			{ Keyword::BCIsConstant, "is_constant", KeywordFlags() },
			{ Keyword::BCInitialValue, "initial_value", KeywordFlags() },
			{ Keyword::BCPacked, "packed", KeywordFlags() },
			{ Keyword::BCCPointer, "cpointer", KeywordFlags() },
		});

	// `- 1` because of `Keyword::NotAKeyword`
	static_assert(
		static_cast<usize>(Keyword::COUNT) - 1
		== LANG_KEYWORDS_ARRAY.size() + BC_KEYWORDS_ARRAY.size()
	);

	constexpr auto SPECIAL_ARRAY = std::to_array<std::pair<Special, std::string_view>>({
		{ Special::NotASpecial, "NotASpecial" },
		{ Special::Semicolon, ";" },
		{ Special::Comma, "," },
		{ Special::AtSign, "@" },
		{ Special::HashSign, "#" },
		{ Special::DollarSign, "$" },
		{ Special::Underscore, "_" },
	});

	constexpr auto OPERATOR_ARRAY = std::to_array<std::pair<NamedOperator, std::string_view>>({
		{ NamedOperator::NotAnOperator, "NotAnOperator" },

		{ NamedOperator::As, "as" },
		{ NamedOperator::Period, "." },
		{ NamedOperator::Range, ".." },
		{ NamedOperator::PeriodQuestion, ".?" },
		{ NamedOperator::PeriodStar, ".*" },
		{ NamedOperator::Colon, ":" },
		{ NamedOperator::Reflect, "::" },
		{ NamedOperator::Assign, "=" },
		{ NamedOperator::QuestionMark, "?" },
		{ NamedOperator::SingleArrow, "->" },
		{ NamedOperator::DoubleArrow, "=>" },

		{ NamedOperator::Pipe, "|" },
		{ NamedOperator::Ampersand, "&" },
		{ NamedOperator::BitXor, "^" },
		{ NamedOperator::BitNot, "~" },

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

	constexpr auto NUMERIC_LITERAL_TYPE_SPECIFIER_ARRAY
		= std::to_array<std::pair<NumericLiteralTypeSpecifier, std::string_view>>({
			{ NumericLiteralTypeSpecifier::NotATypeSpecifier, "NotATypeSpecifier" },
			{ NumericLiteralTypeSpecifier::i8, "i8" },
			{ NumericLiteralTypeSpecifier::i16, "i16" },
			{ NumericLiteralTypeSpecifier::i32, "i32" },
			{ NumericLiteralTypeSpecifier::i64, "i64" },
			{ NumericLiteralTypeSpecifier::i128, "i128" },

			{ NumericLiteralTypeSpecifier::u8, "u8" },
			{ NumericLiteralTypeSpecifier::u16, "u16" },
			{ NumericLiteralTypeSpecifier::u32, "u32" },
			{ NumericLiteralTypeSpecifier::u64, "u64" },
			{ NumericLiteralTypeSpecifier::u128, "u128" },

			{ NumericLiteralTypeSpecifier::f16, "f16" },
			{ NumericLiteralTypeSpecifier::f32, "f32" },
			{ NumericLiteralTypeSpecifier::f64, "f64" },
			{ NumericLiteralTypeSpecifier::f128, "f128" },

		});

	// Distinct for all keyword modes:
	base::VectorMap<base::StrID, Keyword, false, true> lang_keyword_map;
	base::VectorMap<base::StrID, Keyword, false, true> bc_keyword_map;

	// Single for all
	base::VectorMap<Keyword, base::StrID, false, true> rev_keyword_map;

	// Single for all:
	base::VectorMap<Keyword, KeywordFlags, false, true> keyword_flags;

	base::VectorMap<base::StrID, Special, false, true>                     special_map;
	base::VectorMap<base::StrID, NamedOperator, false, true>               operator_map;
	base::VectorMap<base::StrID, NumericLiteralTypeSpecifier, false, true> numeric_specifier_map;

	base::VectorMap<Special, base::StrID, false, true>       rev_special_map;
	base::VectorMap<NamedOperator, base::StrID, false, true> rev_operator_map;
	base::VectorMap<NumericLiteralTypeSpecifier, base::StrID, false, true> rev_numeric_specifier_map;

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

		// numeric literal type suffixes:
		for (auto [k, s]: NUMERIC_LITERAL_TYPE_SPECIFIER_ARRAY) {
			numeric_specifier_map.put(makeStrID(s), k);
			rev_numeric_specifier_map.put(k, makeStrID(s));
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
		CORE_ASSERT(
			NUMERIC_LITERAL_TYPE_SPECIFIER_ARRAY.size() == numeric_specifier_map.size(),
			NUMERIC_LITERAL_TYPE_SPECIFIER_ARRAY.size() == rev_numeric_specifier_map.size(),
			"numeric specifier map error"
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
	}

	Special strAsSpecial(base::StrID id) {
		if (special_map.contains(id)) return special_map[id];
		return Special::NotASpecial;
	}

	NamedOperator strAsOperator(base::StrID id) {
		if (operator_map.contains(id)) return operator_map[id];
		return NamedOperator::NotAnOperator;
	}

	NumericLiteralTypeSpecifier strAsNumericLiteralTypeSpecifier(base::StrID id) {
		if (numeric_specifier_map.contains(id)) return numeric_specifier_map[id];
		return NumericLiteralTypeSpecifier::NotATypeSpecifier;
	}

	base::StrID keywordToStr(Keyword key) { return rev_keyword_map[key]; }

	base::StrID specialToStr(Special spec) { return rev_special_map[spec]; }

	base::StrID operatorToStr(NamedOperator oper) { return rev_operator_map[oper]; }

	base::StrID numericLiteralTypeSpecifierToStr(NumericLiteralTypeSpecifier oper) {
		return rev_numeric_specifier_map[oper];
	}

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

	std::vector<NumericLiteralTypeSpecifier> getNumericTypeSpecifiers() {
		std::vector<NumericLiteralTypeSpecifier> result;
		result.reserve(NUMERIC_LITERAL_TYPE_SPECIFIER_ARRAY.size());
		for (const auto& [k, s]: NUMERIC_LITERAL_TYPE_SPECIFIER_ARRAY) result.push_back(k);
		return result;
	}
}
