#pragma once

#include <base/extend_cpp/stringifyable_enum.hpp>

namespace pst {
	/**
	 * PST element kind.
	 * @note Same groups of elements are under the same kind.
	 * @note Not a full list, since we don't need it for all elements yet.
	 * We can extend/specialize it when needed.
	 * @todo: sort it?
	 */
	enum class ElementKind {
		TopLevel,
		Import,

		// Import Chains
		ImportIdentifierAs,
		ImportStarHides,
		ImportNested,

		StmtSpecifier,

		CodeBlock,
		CodeBlockOrStmt,

		// Duckling declarations:
		Namespace,
		Class,
		Variable,
		Fun,
		FunDecl,
		Pattern,
		Block,
		SpecifierBlock,
		TemplateStmt,

		Using,
		Alias,

		Const,

		Expand,

		Attribute,

		// Duckling statements:
		If,
		While,
		For,

		// Actions:
		Action,

		// Expressions:
		ExprStmt,
		Match,
		MatchCase,

		// Expression wrappers:
		RoundGroupExpr,
		CallList,
		TemplateList,

		// for all expression elements:
		ExprElement,

		// for all Expr holders:
		ExprHolder,

		// classes:
		ClassField,
		ClassMethod,
		ClassSpecifierBlock,
		ClassSpecial,

		// use it, once its docs are more stable:
		ClassConstructor,
		ClassDestructor,

		// others:
		Param,
		ParamList,
		InheritanceList,
		FlowPatternList,
		DottedName,
		CallArgument,
		NestedImportList,
		AtrArgList,

		TemplateDecl,

		// wrappers
		OperatorWrapper,
		KeywordWrapper,
		IdentifierWrapper,

		FormatSubExpression,
		FormatSubString,

		// patterns:
		FlowPattern,
		AnalysisPattern,
		DeconstructorPattern,
		TuplePattern,
		WildcardPattern,
		BindingPattern,
		ValuePattern,


		// for detecting when kind was not set:
		KindNotSet,
	};
}
