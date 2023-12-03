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

namespace rift_def {

	enum class KeywordMode {
		RiftSource,
		RiftBC,
	};
	constexpr KeywordMode DEFAULT_MODE = KeywordMode::RiftSource;


	enum class Keyword {
		NotAKeyword,

		// Non-code declaration
		// @TODO: struct or class?
		Fun,
		Struct,
		Namespace,
		Import,
		Using,
		Alias,

		// Var-like:
		Var,
		Let,
		Const,

		// Control-flow:
		While,
		For,
		Loop,
		If,
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
		Float,
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

		// Access specifiers:
		Public,
		Private,  // ...

		// Misc:

		// Rift Test:
		RiftTestEagerLookup,


		// BC:
		BCFunction,
		BCLocalSize,
		BCRetSize,
		BCArgSize,
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
		DolarSign,
		HashSign,
		//...
	};

	// only operator significant during parsing
	enum class Operator {
		NotAnOperator,
		Period,
		PeriodStar,
		Comma,
		Colon,
		Assign,
		QuestionMark,
		SingleArrow,
		DoubleArrow,

		Plus,
		Minus,
		DoublePlus,
		DoubleMinus,
		Multiply,
		Divide,
	};

	// @TODO: this could be defined enum-like, maybe with macro
	namespace KeywordFlags {
		constexpr base::FlagType is_action(0);
	}

	namespace key_spec_op {
		void init();
	}

	void setKeywordMode(KeywordMode mode);

	Special  strAsSpecial(base::StrId id);
	Keyword  strAsKeyword(base::StrId id);
	Operator strAsOperator(base::StrId id);

	base::StrId keywordToStr(Keyword key);
	base::StrId specialToStr(Special spec);
	base::StrId operatorToStr(Operator oper);

	base::FlagType keywordFlags(Keyword key);

	std::vector<Keyword> getKeywords();
}
