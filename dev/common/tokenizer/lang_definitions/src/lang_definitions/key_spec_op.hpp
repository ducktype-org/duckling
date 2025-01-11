/**
 * @file keywords.hpp
 * @brief
 * Moduł pozwalający wykrywać i definiować keyword-y
 *
 *
 * Usage:
 * keywords::init() should be called before anything else (including lexer), and only once
 * getKeyword return keyword based, on `keywords_array` inside cpp
 * if given RawView does not represent keyword NotAKeyword is returned.
 */
#pragma once

#include <base/string_id.hpp>
#include <base/flag.hpp>

// @TODO: Implement reflection for those enums

namespace lang_def {

	enum class KeywordMode {
		DucklingSource,
		DuckBC,
	};
	constexpr KeywordMode DEFAULT_MODE = KeywordMode::DucklingSource;


	enum class Keyword {
		NotAKeyword,

		// Non-code declaration
		// @TODO: struct or class?
		Fun,
		Class,
		Namespace,
		Import,
		As,
		Using,
		Alias,
		In,
		Lambda,

		// Var-like:
		Var,
		Let,
		Const,

		// Control-flow:
		While,
		For,
		Loop,
		If,
		Then,
		Elif,
		Else,

		// Other block:
		Block,
		With,
		Try,
		Catch,
		Test,
		Debug,
		Switch,
		Case,

		// Actions:
		Return,
		Break,
		Continue,
		Redo,
		Restart,
		Defer,
		// @TODO: catch/rescue, raise/throw
		Throw,
		// @TODO: is assert a keyword?
		Assert,
		CompileAssert,

		// Types:
		// @IDEA: change i -> s

		// NOLINTBEGIN
		// no list, since those keywords do not much
		// identifier naming rules.

		i8,
		i16,
		i32,
		i64,
		i128,
		u8,
		u16,
		u32,
		u64,
		u128,

		f16,
		f32,
		f64,
		f80,
		f128,

		// NOLINTEND

		Char,
		Bool,  // ...

		// @TODO: do we need all of them?
		Vec,
		Set,
		Dict,
		Array,

		// Const values:
		None,
		True,
		False,

		// Literal Operators:
		// @TODO: operators or functions?
		// @TODO: | or bitor, || or or?
		Sizeof,
		Not,
		And,
		Or,
		Xor,

		// Class specific:
		Public,
		Private,
		Protected,
		Static,
		This,
		Extends,
		Implements,

		// Misc:

		// Duckling Test:

		// BC:
		BCFunction,
		BCLocalSize,
		BCRetSize,
		BCArgSize,
		BCNextArgSize,
		BCDefine,
		BCLabel,
		BCArg,
		BCLocal,
		BCCode,
		BCType,
		BCPrimitive,
		BCPointer,
		BCStaticTable,
		BCDynamicTable,
		BCData,
		BCVariant,
		BCFunType,
	};

	enum class Special {
		NotASpecial,
		Semicolon,
		AtSign,
		Comma,
		DolarSign,
		HashSign,
		//...
	};

	// only operator significant during parsing
	enum class NamedOperator {
		NotAnOperator,
		Period,
		PeriodStar,
		Colon,
		Assign,
		Pipe,  // | for variants and bitwise or.
		QuestionMark,
		SingleArrow,
		DoubleArrow,

		Lesser,
		Greater,
		LEqual,
		GEqual,
		Equal,
		NotEqual,

		Plus,
		Minus,
		DoublePlus,
		DoubleMinus,
		Multiply,
		Divide,
	};
}

MAKE_FLAG_TYPE(lang_def, KeywordFlagsOptions, KeywordFlags, IS_ACTION)

namespace lang_def {
	namespace key_spec_op {
		void init();
	}

	void setKeywordMode(KeywordMode mode);

	Special       strAsSpecial(base::StrID id);
	Keyword       strAsKeyword(base::StrID id);
	NamedOperator strAsOperator(base::StrID id);

	base::StrID keywordToStr(Keyword key);
	base::StrID specialToStr(Special spec);
	base::StrID operatorToStr(NamedOperator oper);

	KeywordFlags keywordFlags(Keyword key);

	std::vector<Keyword>       getKeywords();
	std::vector<Special>       getSpecials();
	std::vector<NamedOperator> getOperators();
}
