/**
 * @file keywords.hpp
 * @brief
 * Module allowing for detecting and defining keywords.
 *
 *
 * Usage:
 * keywords::init() should be called before anything else (including lexer), and only once
 * getKeyword return keyword based, on `keywords_array` inside cpp
 * if given RawView does not represent keyword NotAKeyword is returned.
 */
#pragma once

#include <base/extend_cpp/flag.hpp>

#include <init/init.hpp>
#include <string_id/string_id.hpp>

namespace lang_def {

	enum class KeywordMode {
		DucklingSource,
		DuckBC,
	};
	constexpr KeywordMode DEFAULT_MODE = KeywordMode::DucklingSource;


	enum class Keyword {
		NotAKeyword,

		// Non-code declaration
		Fun,
		FunDecl,
		Pattern,
		Class,
		Namespace,
		Import,
		Hides,
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
		Else,

		// Other block:
		Block,
		With,
		Try,
		Catch,
		Test,
		Debug,
		Match,
		Switch,
		Case,

		// Macro
		Expand,

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
		// no lint, since those keywords do not follow
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
		Bool,
		Str,
		Type,  // ...

		// @TODO: do we need all of them?
		List,
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

		// General text prefix operators (Not doesn't count)
		Len,  // @TODO: #1970 This being an operator may be temporary. This should probably be
		      // removed one we can use builtin methods/fields.
		Ref,
		Box,
		Copy,
		Move,
		Refof,

		Destroy,

		// Class specific:
		Public,
		Private,
		Protected,
		Static,
		Self,
		Extends,
		Implements,

		// Stmt specifiers:
		Extern,

		// Misc:

		// Duckling Test:

		// BC:
		BCFunction,
		BCType,
		BCPrimitive,
		BCPointer,
		BCFixedSizeTable,
		BCDynamicTable,
		BCData,
		BCVariant,
		BCFunType,
		BCGlobalData,
		BCGlobalConstructor,
		BCGlobalDestructor,
		BCOpaque,
		BCClass,
		BCAbstract,
		BCInterface,
		BCExtends,
		BCImplements,
		BCVirtualMethods,
		BCFields,
		BCMethodImplementations,
		BCTrue,
		BCFalse,
		COUNT,
	};

	enum class Special {
		NotASpecial,
		Semicolon,
		AtSign,
		Comma,
		DollarSign,
		HashSign,
		Underscore,
		//...
	};

	// only operator significant during parsing
	enum class NamedOperator {
		NotAnOperator,

		Period,
		Range,
		PeriodQuestion,
		PeriodStar,
		Colon,
		Reflect,
		Assign,
		QuestionMark,
		SingleArrow,
		DoubleArrow,

		Pipe,       // | for variants and bitwise or.
		Ampersand,  // & for references and bitwise and.
		BitXor,

		LeftShift,
		RightShift,

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
		Remainder,
		Exponentiate,
	};

	enum class NumericLiteralTypeSpecifier {
		NotATypeSpecifier,
		// NOLINTBEGIN
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
		f128
		// NOLINTEND
	};
}

MAKE_FLAG_TYPE(lang_def, KeywordFlagsOptions, KeywordFlags, IsAction, IsGenPrefixOp, IsStmtStart, IsSpecifier)

namespace lang_def {
	namespace key_spec_op {
		/**
		 * @brief Initializes the key_spec_op module.
		 * It will run automagically when InitObject is used.
		 */
		void init();

		// note: it might be valid to put this init in cpp
		// but it is safer to have it here.
		RUN_BEFORE_MAIN(init::registerForInit(key_spec_op::init));
	}

	void        setKeywordMode(KeywordMode mode);
	KeywordMode getKeywordMode();

	Special                     strAsSpecial(base::StrID id);
	Keyword                     strAsKeyword(base::StrID id);
	NamedOperator               strAsOperator(base::StrID id);
	NumericLiteralTypeSpecifier strAsNumericLiteralTypeSpecifier(base::StrID id);

	base::StrID keywordToStr(Keyword key);
	base::StrID specialToStr(Special spec);
	base::StrID operatorToStr(NamedOperator oper);
	base::StrID numericLiteralTypeSpecifierToStr(NumericLiteralTypeSpecifier oper);

	KeywordFlags keywordFlags(Keyword key);

	std::vector<Keyword>                     getKeywords();
	std::vector<Special>                     getSpecials();
	std::vector<NamedOperator>               getOperators();
	std::vector<NumericLiteralTypeSpecifier> getNumericTypeSpecifiers();
}
