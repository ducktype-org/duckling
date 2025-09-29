#pragma once

#include <base/stringifyable_enum.hpp>

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

		StmtSpecifier,

		CodeBlock,
		CodeBlockOrStmt,

		ClassBlock,

		// Duckling declarations:
		Namespace,
		Class,
		Variable,
		Fun,
		FunDecl,
		Pattern,
		Block,

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

		// for all expression elements:
		ExprElement,

		// for all Expr holders:
		ExprHolder,

		// classes:
		ClassField,
		ClassMethod,
		AccessBlock,
		NonClassStmt,
		ClassSpecial,

		// use it, once its docs are more stable:
		// ClassConstructor,
		// ClassDestructor,

		// note: AccessBlock is not here, since it should be invisible to HELIOS (at least for now)

		// others:
		Param,
		ParamList,
		FlowPatternList,
		DottedName,
		CallArgument,

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
